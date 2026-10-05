#include "MCMBridge/Papyrus/NativeHostUI.h"

#include "MCMBridge/Papyrus/NativeFacade.h"
#include "MCMBridge/Plugin/BridgeController.h"

namespace
{
	bool IsOpenGetter(const RE::BSScript::StackFrame& a_frame)
	{
		const auto function = a_frame.owningFunction;
		if (!function || function->GetObjectTypeName() != "nl_mcm" ||
			function->GetFunctionType() != RE::BSScript::IFunction::FunctionType::kGetter ||
			function->GetParamCount() != 0 || !a_frame.self.IsObject())
			return false;
		// Property getter names are compiler details. Verify the actual linked
		// property/function association instead of guessing a generated name.
		for (const auto* type = a_frame.owningObjectType.get(); type; type = type->GetParent()) {
			if (!type->IsLinked())
				return false;
			const auto* properties = type->GetPropertyIter();
			for (std::uint32_t index = 0; index < type->GetNumProperties(); ++index)
				if (properties[index].name == "IsMCMOpen" && properties[index].info.getFunction == function)
					return true;
		}
		return false;
	}
}

namespace MCMBridge::NativeHostUI
{
	bool InterceptExternalState(const RE::BSScript::StackFrame& a_frame, std::string_view a_menu,
		std::string_view a_target, RE::BSScript::Variable& a_result)
	{
		if (!BridgeController::GetSingleton().IsNativeHost() || a_menu != "Journal Menu" ||
			(!a_target.empty() && a_target != "_root.ConfigPanelFader.configPanel.contentHolder.modListPanel.decorTitle.textHolder.textField.text"))
			return false;
		const auto* caller = a_frame.previousFrame;
		if (!caller || !IsOpenGetter(*caller))
			return false;
		const auto script = caller->self.GetObject();
		if (!script)
			return false;
		const auto token = NativeFacadeSession().TokenForOwner(reinterpret_cast<std::uintptr_t>(script.get()));
		const bool active = NativeFacadeSession().IsActive(token);
		if (a_target.empty()) {
			a_result.SetBool(active);
			return true;
		}
		const auto* name = script->GetProperty("ModName");
		if (!name || !name->IsString())
			return false;
		const auto text = active ? name->GetString() : name->GetString().empty() ? std::string_view("native-host") :
		                                                                           std::string_view{};
		a_result.SetString(RE::BSFixedString(text));
		return true;
	}
}
