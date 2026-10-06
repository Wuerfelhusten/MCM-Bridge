#include "MCMBridge/Papyrus/UIInvokeStringArrayBridge.h"

#include "MCMBridge/Core/ScopedFrontendRedraw.h"
#include "MCMBridge/Papyrus/NativeHostProtocol.h"
#include "MCMBridge/Papyrus/SkyUIFrontendState.h"
#include "MCMBridge/Plugin/BridgeController.h"
#include "MCMBridge/UI/MessageDialog.h"

#include <type_traits>

namespace
{
	std::string CopyString(const RE::BSFixedString& a_value)
	{
		const auto* text = a_value.c_str();
		return text ? std::string(text) : std::string{};
	}

	bool IsTarget(std::string_view a_target, std::string_view a_suffix)
	{
		return a_target.size() >= a_suffix.size() && a_target.ends_with(a_suffix);
	}

	template <class BuildValues>
	void ForwardToScaleform(std::string a_menuName, std::string a_target, BuildValues a_buildValues)
	{
		auto* tasks = SKSE::GetTaskInterface();
		if (!tasks) {
			return;
		}
		// OpenCustomMenu queues its opening. Like SKSE's original UI.Invoke
		// natives, resolve the movie only when the UI queue executes the call.
		// A submission-time menu check would drop its initial content payload.
		tasks->AddUITask([menuName = std::move(a_menuName),
							 target = std::move(a_target),
							 buildValues = std::move(a_buildValues)]() mutable {
			auto* ui = RE::UI::GetSingleton();
			if (!ui) {
				return;
			}
			auto menu = ui->GetMenu(menuName);
			if (!menu || !menu->uiMovie) {
				return;
			}
			auto values = buildValues(*menu->uiMovie);
			menu->uiMovie->InvokeNoReturn(target.c_str(), values.data(), static_cast<std::uint32_t>(values.size()));
		});
	}

	void InvokeString(
		RE::StaticFunctionTag*,
		RE::BSFixedString a_menuName,
		RE::BSFixedString a_target,
		RE::BSFixedString a_argument)
	{
		auto ownedTarget = CopyString(a_target);
		auto ownedArgument = CopyString(a_argument);
		MCMBridge::SkyUIFrontendState::GetSingleton().ObserveString(ownedTarget, ownedArgument);
		ForwardToScaleform(CopyString(a_menuName), std::move(ownedTarget), [value = std::move(ownedArgument)](RE::GFxMovieView& a_movie) {
			std::vector<RE::GFxValue> values(1);
			a_movie.CreateString(std::addressof(values[0]), value.c_str());
			return values;
		});
	}

	void InvokeBool(
		RE::StaticFunctionTag*,
		RE::BSFixedString a_menuName,
		RE::BSFixedString a_target,
		bool              a_argument)
	{
		auto ownedTarget = CopyString(a_target);
		if (IsTarget(ownedTarget, ".forcePageReset") ||
			(IsTarget(ownedTarget, ".invalidateOptionData") && !MCMBridge::ScopedFrontendRedraw::Owns(CopyString(a_menuName), ownedTarget))) {
			MCMBridge::SkyUIFrontendState::GetSingleton().InvalidatePage();
			MCMBridge::BridgeController::GetSingleton().NotifyFrontendInvalidation();
		}
		ForwardToScaleform(CopyString(a_menuName), std::move(ownedTarget), [a_argument](RE::GFxMovieView&) {
			std::vector<RE::GFxValue> values(1);
			values[0].SetBoolean(a_argument);
			return values;
		});
	}

	void InvokeStringArray(
		RE::StaticFunctionTag*,
		RE::BSFixedString              a_menuName,
		RE::BSFixedString              a_target,
		std::vector<RE::BSFixedString> a_arguments)
	{
		auto                     ownedMenuName = CopyString(a_menuName);
		auto                     ownedTarget = CopyString(a_target);
		std::vector<std::string> ownedArguments;
		ownedArguments.reserve(a_arguments.size());
		for (const auto& argument : a_arguments) {
			ownedArguments.push_back(CopyString(argument));
		}

		if (IsTarget(ownedTarget, ".showMessageDialog") && MCMBridge::MessageDialog::Handle(ownedArguments)) {
			return;
		}
		MCMBridge::BridgeController::GetSingleton().ObserveMenuOptions(
			ownedMenuName, ownedTarget, ownedArguments);
		ForwardToScaleform(std::move(ownedMenuName), std::move(ownedTarget), [values = std::move(ownedArguments)](RE::GFxMovieView& a_movie) {
			std::vector<RE::GFxValue> result(values.size());
			for (std::size_t index = 0; index < values.size(); ++index) {
				a_movie.CreateString(std::addressof(result[index]), values[index].c_str());
			}
			return result;
		});
	}
	// Replacement natives must preserve the signatures declared by SKSE's UI.psc.
	static_assert(std::is_same_v<decltype(&InvokeString),
		void (*)(RE::StaticFunctionTag*, RE::BSFixedString, RE::BSFixedString, RE::BSFixedString)>);
	static_assert(std::is_same_v<decltype(&InvokeBool),
		void (*)(RE::StaticFunctionTag*, RE::BSFixedString, RE::BSFixedString, bool)>);
	static_assert(std::is_same_v<decltype(&InvokeStringArray),
		void (*)(RE::StaticFunctionTag*, RE::BSFixedString, RE::BSFixedString, std::vector<RE::BSFixedString>)>);
}

namespace MCMBridge::UIInvokeStringArrayBridge
{
	bool Register(RE::BSScript::IVirtualMachine* a_virtualMachine)
	{
		if (!a_virtualMachine) {
			SKSE::log::error("Could not install the SkyUI frontend bridge because the Papyrus VM is unavailable");
			return false;
		}
		a_virtualMachine->RegisterFunction("InvokeString", "UI", InvokeString, true);
		a_virtualMachine->RegisterFunction("InvokeBool", "UI", InvokeBool, true);
		a_virtualMachine->RegisterFunction("InvokeStringA", "UI", InvokeStringArray, true);
		if (!NativeHostProtocol::Register(a_virtualMachine))
			SKSE::log::warn("Native host buffer observation is incomplete; script buffer reads remain required");
		SKSE::log::info("Installed the transparent SkyUI frontend bridge");
		return true;
	}
}
