#include "MCMBridge/Papyrus/HelperCustomCapture.h"
#include "MCMBridge/Papyrus/HelperBinaryAdmission.h"

#include "MCMBridge/Core/HelperCustomContent.h"
#include "MCMBridge/Core/HelperMenuCapture.h"
#include "MCMBridge/Papyrus/NativeFacade.h"

#include <MinHook.h>

namespace
{
	using Object = RE::BSTSmartPointer<RE::BSScript::Object>;
	struct CustomDraw
	{
		static void thunk(const std::byte* a_content, const Object& a_object)
		{
			auto&      host = MCMBridge::NativeFacadeSession();
			const auto token = host.TokenForOwner(reinterpret_cast<std::uintptr_t>(a_object.get()));
			if (!token)
				return;
			try {
				if (!a_content)
					return;
				auto content = MCMBridge::CopyHelperCustomContent({ a_content, 48 });
				if (!content) {
					SKSE::log::error("Native Helper custom content rejected: {}", content.error().message);
					return;
				}
				// Draw retains the owning script; the lower LoadCustomContent call does not.
				host.SetCustomContent(token, std::move(content->source), content->x, content->y);
			} catch (const std::exception& error) {
				SKSE::log::error("Native Helper custom capture failed: {}", error.what());
			}
		}
		static inline decltype(&thunk) func{};
	};
}

namespace MCMBridge
{
	bool InstallHelperCustomCapture()
	{
		if (CustomDraw::func)
			return true;
		const auto module = GetModuleHandleW(L"MCMHelper.dll");
		if (!module)
			return true;
		const auto* profile = LoadedHelperBinaryProfile();
		if (!profile)
			return false;
		auto*      base = reinterpret_cast<std::byte*>(module);
		auto*      target = base + profile->Function(HelperHook::kCustom).offset;
		const auto initialized = MH_Initialize();
		if (initialized != MH_OK && initialized != MH_ERROR_ALREADY_INITIALIZED)
			return false;
		void* trampoline{};
		if (MH_CreateHook(target, reinterpret_cast<void*>(&CustomDraw::thunk), &trampoline) != MH_OK)
			return false;
		CustomDraw::func = reinterpret_cast<decltype(CustomDraw::func)>(trampoline);
		if (MH_EnableHook(target) != MH_OK) {
			MH_RemoveHook(target);
			CustomDraw::func = nullptr;
			return false;
		}
		SKSE::log::info("Native Helper custom content capture installed for {}", profile->name);
		return true;
	}
}
