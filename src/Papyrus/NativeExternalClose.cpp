#include "MCMBridge/Papyrus/NativeHostUI.h"

#include "MCMBridge/Papyrus/NativeFacade.h"
#include "MCMBridge/Plugin/BridgeController.h"

namespace MCMBridge::NativeHostUI
{
	bool InterceptExternalClose(const RE::BSScript::StackFrame& a_frame, std::string_view a_menu,
		std::string_view a_target, RE::BSScript::Variable& a_result)
	{
		auto& controller = BridgeController::GetSingleton();
		if (!controller.IsNativeHost() || a_menu != "Journal Menu" ||
			a_target != "_root.ConfigPanelFader.configPanel.contentHolder.modListPanel.decorTitle.textHolder.textField.text")
			return false;
		const auto* caller = a_frame.previousFrame;
		if (!caller || !caller->owningFunction || !caller->self.IsObject() ||
			caller->owningFunction->GetObjectTypeName() != "nl_mcm" || caller->owningFunction->GetName() != "CloseMCM" ||
			caller->owningFunction->GetParamCount() != 1)
			return false;
		const auto& close = caller->GetStackFrameVariable(0, caller->GetPageForFrame());
		if (!close.IsBool())
			return false;
		auto script = caller->self.GetObject();
		if (!script)
			return false;
		const auto* name = script->GetProperty("ModName");
		if (!name || !name->IsString())
			return false;
		// Let the library's existing early-return path release _ctd_lock. Its
		// remaining body only manipulates the Journal and waits for a UI cooldown.
		a_result.SetString(RE::BSFixedString(name->GetString().empty() ? "native-host" : ""));
		const auto token = NativeFacadeSession().TokenForOwner(reinterpret_cast<std::uintptr_t>(script.get()));
		const auto identity = NativeFacadeSession().ReadIdentity(token);
		if (!identity)
			return true;
		const auto revision = controller.CaptureScriptView(identity->modID);
		if (!revision)
			return true;
		if (auto* tasks = SKSE::GetTaskInterface()) {
			tasks->AddTask([script = std::move(script), identity = *identity, revision = *revision, closeFrontend = close.GetBool()] {
				auto& current = BridgeController::GetSingleton();
				if (!current.IsNativeHost() || NativeFacadeSession().Session() != identity.session)
					return;
				const auto registered = current.NativeRegistry().ResolveIdentity(script);
				if (!registered || *registered != identity.modID)
					return;
				current.CloseScriptView(identity.session, revision, identity.modID, closeFrontend);
			});
		}
		return true;
	}
}
