#include "MCMBridge/Papyrus/HostCallArguments.h"

#include <cmath>

namespace MCMBridge
{
	Result<ClassicCall> DecodeHostCall(const MCMHostCall& a_call)
	{
		const auto invalid = std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Invalid host call or arguments" });
		if (!a_call.function || !a_call.mcm_id || !*a_call.mcm_id || a_call.argument_count > 4 ||
			(a_call.argument_count && !a_call.arguments) || !a_call.timeout_ms || a_call.accept_confirmation > 1)
			return invalid;
		ClassicCall result;
		const auto  method = std::string_view(a_call.function);
		bool        found{};
		for (const auto candidate : { ClassicMethod::kOpenConfig, ClassicMethod::kCloseConfig, ClassicMethod::kSetPage,
				 ClassicMethod::kRequestSliderDialogData, ClassicMethod::kRequestMenuDialogData,
				 ClassicMethod::kRequestColorDialogData, ClassicMethod::kRequestInputDialogData,
				 ClassicMethod::kSelectOption, ClassicMethod::kResetOption, ClassicMethod::kSetSliderValue,
				 ClassicMethod::kSetMenuIndex, ClassicMethod::kSetColorValue, ClassicMethod::kSetInputText,
				 ClassicMethod::kRemapKey, ClassicMethod::kHighlightOption, ClassicMethod::kSetModSettingInt, ClassicMethod::kOnSettingChange }) {
			if (ClassicMethodName(candidate) == method) {
				result.method = candidate;
				found = true;
				break;
			}
		}
		if (!found)
			return invalid;
		const auto signature = [&](std::initializer_list<std::uint32_t> a_types) {
			if (a_call.argument_count != a_types.size())
				return false;
			std::size_t index{};
			for (const auto type : a_types) {
				const auto& argument = a_call.arguments[index++];
				if (argument.type != type || (type == MCM_HOST_STRING && !argument.text) ||
					(type == MCM_HOST_FLOAT && !std::isfinite(argument.number)))
					return false;
			}
			return true;
		};
		switch (result.method) {
		case ClassicMethod::kOpenConfig:
		case ClassicMethod::kCloseConfig:
			if (!signature({}))
				return invalid;
			break;
		case ClassicMethod::kSetPage:
		case ClassicMethod::kSetModSettingInt:
			if (!signature({ MCM_HOST_STRING, MCM_HOST_INTEGER }))
				return invalid;
			result.text = a_call.arguments[0].text;
			result.integer = a_call.arguments[1].integer;
			break;
		case ClassicMethod::kSetSliderValue:
			if (!signature({ MCM_HOST_FLOAT }))
				return invalid;
			result.number = a_call.arguments[0].number;
			break;
		case ClassicMethod::kSetInputText:
		case ClassicMethod::kOnSettingChange:
			if (!signature({ MCM_HOST_STRING }))
				return invalid;
			result.text = a_call.arguments[0].text;
			break;
		case ClassicMethod::kRemapKey:
			if (!signature({ MCM_HOST_INTEGER, MCM_HOST_INTEGER, MCM_HOST_STRING, MCM_HOST_STRING }))
				return invalid;
			result.integer = a_call.arguments[0].integer;
			result.secondaryInteger = a_call.arguments[1].integer;
			result.text = a_call.arguments[2].text;
			result.secondaryText = a_call.arguments[3].text;
			break;
		default:
			if (!signature({ MCM_HOST_INTEGER }))
				return invalid;
			result.integer = a_call.arguments[0].integer;
			break;
		}
		return result;
	}
}
