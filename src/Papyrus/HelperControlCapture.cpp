#include "MCMBridge/Papyrus/HelperControlCapture.h"
#include "MCMBridge/Papyrus/HelperBinaryAdmission.h"

#include "MCMBridge/Core/HelperMenuCapture.h"
#include "MCMBridge/Papyrus/NativeFacade.h"

#include <MinHook.h>
#include <limits>

namespace
{
	using Object = RE::BSTSmartPointer<RE::BSScript::Object>;
	// Verified release string_view ABI; never call methods on a foreign STL object.
	struct TextView
	{
		const char* data;
		std::size_t size;
	};
	static_assert(sizeof(TextView) == 16);
	static_assert(sizeof(Object) == 8);

	void Update(const Object& a_object, std::int32_t a_index, std::optional<TextView> a_text,
		std::optional<float> a_value, std::optional<std::int32_t> a_flags)
	{
		auto&      host = MCMBridge::NativeFacadeSession();
		const auto token = host.TokenForOwner(reinterpret_cast<std::uintptr_t>(a_object.get()));
		if (!token || a_index < 0 || a_index >= 128 ||
			(a_text && ((!a_text->data && a_text->size) || a_text->size > std::numeric_limits<std::int32_t>::max())))
			return;
		try {
			std::array<RE::BSTSmartPointer<RE::BSScript::Array>, 3> mirrors;
			const std::array                                        names{ "_optionFlagsBuf", "_strValueBuf", "_numValueBuf" };
			for (std::size_t column = 0; column < names.size(); ++column) {
				const auto* variable = a_object->GetVariable(names[column]);
				if (!variable || !variable->IsArray())
					return;
				mirrors[column] = variable->GetArray();
				if (!mirrors[column] || mirrors[column]->size() != 128)
					return;
			}
			const auto index = static_cast<std::size_t>(a_index);
			auto&      flags = mirrors[0]->data()[index];
			auto&      text = mirrors[1]->data()[index];
			auto&      number = mirrors[2]->data()[index];
			if (!flags.IsInt() || !text.IsString() || !number.IsFloat())
				return;
			std::optional<std::string> copied;
			if (a_text)
				copied = a_text->size ? std::string(a_text->data, a_text->size) : std::string{};
			if (copied && copied->find('\0') != std::string::npos)
				return;
			// Prepare the engine string before changing either copy of the control.
			const RE::BSFixedString engineText(copied ? copied->c_str() : "");
			if (!host.UpdateControl(token, a_index, std::move(copied), a_value, a_flags, flags.GetSInt() & 0xFF))
				return;
			// Only SKI host mirrors, never mod properties. Keep subsequent Helper reads
			// and the callback's final buffer import consistent with the native page.
			if (a_flags)
				flags.SetSInt((flags.GetSInt() & 0xFF) + *a_flags * 256);
			if (a_text)
				text.SetString(engineText);
			if (a_value)
				number.SetFloat(*a_value);
			// Publication is owned by callback completion, including noUpdate batches.
		} catch (const std::exception& error) {
			SKSE::log::error("Native Helper control update rejected: {}", error.what());
		}
	}

	struct Flags
	{
		static void thunk(const Object& a_object, std::int32_t a_option, std::int32_t a_flags, bool)
		{
			Update(a_object, a_option < 0 ? -1 : a_option % 256, {}, {}, a_flags);
		}
		static inline decltype(&thunk) func{};
	};
	struct Number
	{
		static void thunk(const Object& a_object, std::int32_t a_index, float a_value, bool)
		{
			Update(a_object, a_index, {}, a_value, {});
		}
		static inline decltype(&thunk) func{};
	};
	struct Text
	{
		static void thunk(const Object& a_object, std::int32_t a_index, TextView a_text, bool)
		{
			Update(a_object, a_index, a_text, {}, {});
		}
		static inline decltype(&thunk) func{};
	};
	struct Values
	{
		static void thunk(const Object& a_object, std::int32_t a_index, TextView a_text, float a_value, bool)
		{
			Update(a_object, a_index, a_text, a_value, {});
		}
		static inline decltype(&thunk) func{};
	};
}

namespace MCMBridge
{
	bool InstallHelperControlCapture()
	{
		if (Flags::func)
			return true;
		const auto module = GetModuleHandleW(L"MCMHelper.dll");
		if (!module)
			return true;
		const auto* profile = LoadedHelperBinaryProfile();
		if (!profile)
			return false;
		auto* base = reinterpret_cast<std::byte*>(module);
		struct Hook
		{
			std::size_t offset;
			void*       replacement;
			void*       original{};
		};
		std::array hooks{
			Hook{ profile->Function(HelperHook::kFlags).offset, reinterpret_cast<void*>(&Flags::thunk) },
			Hook{ profile->Function(HelperHook::kNumber).offset, reinterpret_cast<void*>(&Number::thunk) },
			Hook{ profile->Function(HelperHook::kText).offset, reinterpret_cast<void*>(&Text::thunk) },
			Hook{ profile->Function(HelperHook::kValues).offset, reinterpret_cast<void*>(&Values::thunk) }
		};
		const auto initialized = MH_Initialize();
		if (initialized != MH_OK && initialized != MH_ERROR_ALREADY_INITIALIZED)
			return false;
		const auto rollback = [&] {
			for (const auto& hook : hooks) {
				if (hook.original) {
					MH_DisableHook(base + hook.offset);
					MH_RemoveHook(base + hook.offset);
				}
			}
			Flags::func = nullptr;
			Number::func = nullptr;
			Text::func = nullptr;
			Values::func = nullptr;
		};
		for (auto& hook : hooks) {
			if (MH_CreateHook(base + hook.offset, hook.replacement, &hook.original) != MH_OK) {
				rollback();
				return false;
			}
		}
		Flags::func = reinterpret_cast<decltype(Flags::func)>(hooks[0].original);
		Number::func = reinterpret_cast<decltype(Number::func)>(hooks[1].original);
		Text::func = reinterpret_cast<decltype(Text::func)>(hooks[2].original);
		Values::func = reinterpret_cast<decltype(Values::func)>(hooks[3].original);
		for (const auto& hook : hooks) {
			if (MH_EnableHook(base + hook.offset) != MH_OK) {
				rollback();
				return false;
			}
		}
		SKSE::log::info("Native Helper control capture installed for {}", profile->name);
		return true;
	}
}
