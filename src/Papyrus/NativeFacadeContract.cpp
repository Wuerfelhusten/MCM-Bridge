#include "MCMBridge/Core/PapyrusIdentifier.h"
#include "MCMBridge/Papyrus/NativeFacade.h"

#include <array>
#include <format>
#include <string_view>
#include <utility>

namespace MCMBridge
{
	bool HasNativeFacadeContract(const RE::BSTSmartPointer<RE::BSScript::Object>& a_script, bool a_manager, std::string* a_reason)
	{
		if (a_reason)
			a_reason->clear();
		const auto reject = [a_reason](std::string a_message) {
			if (a_reason)
				*a_reason = std::move(a_message);
			return false;
		};
		if (!a_script)
			return reject("Script object is unavailable");
		const auto* version = a_script->GetProperty(a_manager ? "MCMBridgeManagerVersion" : "MCMBridgeFacadeVersion");
		if (!version || !version->IsInt() || version->GetSInt() != 1)
			return reject("Facade version property is missing, has the wrong type, or is not 1");
		const std::string_view expectedType = a_manager ? "SKI_ConfigManager" : "SKI_ConfigBase";
		const std::array       required = a_manager ?
		                                      std::array{ std::pair{ "BridgeCallAdmissionContract", 0U }, std::pair{ "BridgeAwait", 1U }, std::pair{ "RegisterMod", 2U }, std::pair{ "UnregisterMod", 1U } } :
		                                      std::array{ std::pair{ "BridgeEnterCall", 0U }, std::pair{ "BridgeLeaveCall", 2U }, std::pair{ "BridgeAwaitCall", 1U }, std::pair{ "BridgePublishBuffers", 0U } };
		// Saved properties are not evidence of installed code. Check the declaring
		// facade, not a similarly named function on a derived mod script.
		for (const auto* type = a_script->GetTypeInfo(); type; type = type->GetParent()) {
			if (!SamePapyrusIdentifier(type->GetName(), expectedType))
				continue;
			if (type->linkedValid != RE::BSScript::ObjectTypeInfo::LinkValidState::kLinkedValid)
				return reject(std::format("Facade {} is not linked-valid (state {})", type->GetName(), static_cast<unsigned>(type->linkedValid)));
			const auto* functions = type->GetMemberFuncIter();
			if (!functions)
				return reject(std::format("Facade {} has no member function table", type->GetName()));
			for (const auto& [name, parameters] : required) {
				bool found{};
				for (std::uint32_t index = 0; index < type->GetNumMemberFuncs(); ++index) {
					const auto& function = functions[index].func;
					if (function && SamePapyrusIdentifier(function->GetName().c_str(), name)) {
						if (function->GetIsNative() || function->GetIsStatic() || function->GetParamCount() != parameters)
							return reject(std::format("Facade {}.{} has incompatible signature: native={}, static={}, parameters={}, expected={}",
								type->GetName(), function->GetName().c_str(), function->GetIsNative(), function->GetIsStatic(), function->GetParamCount(), parameters));
						found = true;
						break;
					}
				}
				if (!found)
					return reject(std::format("Facade {} is missing function {} ({} members)", type->GetName(), name, type->GetNumMemberFuncs()));
			}
			return true;
		}
		return reject(std::format("Declaring facade {} is absent from the loaded script hierarchy", expectedType));
	}
}
