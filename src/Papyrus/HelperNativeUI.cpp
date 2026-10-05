#include "MCMBridge/Papyrus/HelperNativeUI.h"
#include "MCMBridge/Papyrus/HelperBinaryAdmission.h"

#include "MCMBridge/Core/HelperMenuCapture.h"
#include "MCMBridge/Papyrus/NativeFacade.h"

#include <MinHook.h>
#include <limits>

namespace
{
	struct ExternalSpan
	{
		const std::byte* data;
		std::size_t      count;
	};
	static_assert(sizeof(ExternalSpan) == 16);
	using Object = RE::BSTSmartPointer<RE::BSScript::Object>;
	static_assert(sizeof(Object) == 8);
	using Setter = void (*)(const Object&, ExternalSpan);
	Setter original{};

	void SetOptions(const Object& a_object, ExternalSpan a_options)
	{
		auto&      host = MCMBridge::NativeFacadeSession();
		const auto token = host.TokenForOwner(reinterpret_cast<std::uintptr_t>(a_object.get()));
		if (!token) {
			SKSE::log::debug("Discarded Helper menu options without a native execution owner");
			return;
		}
		try {
			const auto request = host.ActiveDialogRequest(token, 5);
			if (!request || a_options.count > std::numeric_limits<std::size_t>::max() / 32 ||
				(!a_options.data && a_options.count)) {
				SKSE::log::error("Native Helper menu options rejected: no matching dialog or invalid span");
				return;
			}
			auto options = MCMBridge::CopyHelperMenuStrings({ a_options.data, a_options.count * 32 });
			if (!options) {
				SKSE::log::error("Native Helper menu options rejected: {}", options.error().message);
				return;
			}
			if (!host.SetDialogOptions(request, std::move(*options)))
				SKSE::log::debug("Discarded Helper menu options after the dialog expired");
		} catch (const std::exception& error) {
			SKSE::log::error("Native Helper menu capture failed: {}", error.what());
		}
	}
}

namespace MCMBridge
{
	bool InstallHelperMenuCapture()
	{
		if (original)
			return true;
		const auto module = GetModuleHandleW(L"MCMHelper.dll");
		if (!module)
			return true;
		const auto* profile = LoadedHelperBinaryProfile();
		if (!profile)
			return false;
		auto*      base = reinterpret_cast<std::byte*>(module);
		auto*      target = base + profile->Function(HelperHook::kMenu).offset;
		const auto initialized = MH_Initialize();
		if (initialized != MH_OK && initialized != MH_ERROR_ALREADY_INITIALIZED)
			return false;
		void*      trampoline{};
		const auto created = MH_CreateHook(target, reinterpret_cast<void*>(&SetOptions), &trampoline);
		if (created != MH_OK) {
			SKSE::log::error("Helper menu capture could not create hook: {}", MH_StatusToString(created));
			return false;
		}
		original = reinterpret_cast<Setter>(trampoline);
		const auto enabled = MH_EnableHook(target);
		if (enabled != MH_OK) {
			MH_RemoveHook(target);
			original = nullptr;
			SKSE::log::error("Helper menu capture could not enable hook: {}", MH_StatusToString(enabled));
			return false;
		}
		SKSE::log::info("Native Helper menu capture installed for {}", profile->name);
		return true;
	}
}
