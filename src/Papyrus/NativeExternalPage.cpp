#include "MCMBridge/Papyrus/NativeHostUI.h"

#include "MCMBridge/Papyrus/NativeFacade.h"
#include "MCMBridge/Plugin/BridgeController.h"

namespace MCMBridge::NativeHostUI
{
	bool InterceptExternalNavigation(const RE::BSScript::StackFrame& a_frame, std::string_view a_menu, std::string_view a_target)
	{
		if (!BridgeController::GetSingleton().IsNativeHost() || a_menu != "Journal Menu" ||
			a_target != "_root.ConfigPanelFader.configPanel.setPageNames")
			return false;
		const auto* caller = a_frame.previousFrame;
		if (!caller || !caller->owningFunction || !caller->self.IsObject() || !caller->self.GetObject() ||
			caller->owningFunction->GetObjectTypeName() != "nl_mcm" ||
			caller->owningFunction->GetName() != "ForcePageListReset" || caller->owningFunction->GetParamCount() != 1 ||
			!caller->GetStackFrameVariable(0, caller->GetPageForFrame()).IsBool())
			return false;
		// Pages remains authoritative and is read by native navigation polling.
		// The following GoToPage call queues selection through the controller.
		// Closed or expired scripts must not send this update to the real Journal.
		return true;
	}

	bool InterceptExternalPage(const RE::BSScript::StackFrame& a_frame, std::string_view a_menu,
		std::string_view a_target, RE::BSScript::Variable& a_result)
	{
		auto& controller = BridgeController::GetSingleton();
		if (!controller.IsNativeHost() || a_menu != "Journal Menu" ||
			a_target != "_root.ConfigPanelFader.configPanel.contentHolder.modListPanel.decorTitle.textHolder.textField.text")
			return false;
		const auto* caller = a_frame.previousFrame;
		if (!caller || !caller->owningFunction || !caller->self.IsObject() ||
			caller->owningFunction->GetObjectTypeName() != "nl_mcm")
			return false;
		const bool rename = caller->owningFunction->GetName() == "_RenameModule";
		if ((!rename && caller->owningFunction->GetName() != "GoToPage") ||
			caller->owningFunction->GetParamCount() != (rename ? 2U : 1U))
			return false;
		const auto& page = caller->GetStackFrameVariable(0, caller->GetPageForFrame());
		if (!page.IsString())
			return false;
		auto        script = caller->self.GetObject();
		const auto* name = script ? script->GetProperty("ModName") : nullptr;
		if (!name || !name->IsString())
			return false;
		std::string requested(page.GetString());
		bool        reselect = true;
		if (rename) {
			const auto& replacement = caller->GetStackFrameVariable(1, caller->GetPageForFrame());
			const auto* current = script->GetVariable("_currentPage");
			if (!replacement.IsString() || !current || !current->IsString())
				return false;
			// The library has already renamed Pages. Preserve the selected raw page
			// unless it was the renamed module; do not repeat the rename itself.
			reselect = current->GetString() == page.GetString();
			requested = replacement.GetString();
		}
		// The original helper exits before its Journal calls. Its raw request is
		// resolved only after active callbacks have finished, never by a stale index.
		a_result.SetString(RE::BSFixedString(name->GetString().empty() ? "native-host" : ""));
		// Navigation polling reads the updated Pages property. Renaming another
		// module must not dispatch an extra OnPageReset for the current page.
		if (!reselect)
			return true;
		const auto token = NativeFacadeSession().TokenForOwner(reinterpret_cast<std::uintptr_t>(script.get()));
		const auto identity = NativeFacadeSession().ReadIdentity(token);
		if (!identity)
			return true;
		const auto revision = controller.CaptureScriptView(identity->modID);
		if (!revision)
			return true;
		if (auto* tasks = SKSE::GetTaskInterface()) {
			tasks->AddTask([script = std::move(script), identity = *identity, revision = *revision, page = std::move(requested)]() mutable {
				auto& current = BridgeController::GetSingleton();
				if (!current.IsNativeHost() || NativeFacadeSession().Session() != identity.session)
					return;
				const auto registered = current.NativeRegistry().ResolveIdentity(script);
				if (!registered || *registered != identity.modID)
					return;
				current.QueueScriptPage(identity.session, revision, identity.modID, std::move(page));
			});
		}
		return true;
	}
}
