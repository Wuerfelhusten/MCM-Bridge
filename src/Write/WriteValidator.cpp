#include "MCMBridge/Write/WriteValidator.h"

namespace MCMBridge
{
	Result<MCMValue> ConfirmWrite(const MCMSnapshot& a_snapshot, const WriteCommand& a_command, const MCMValue& a_effectiveValue)
	{
		const MCMControl* found = nullptr;
		for (const auto& mod : a_snapshot.mods) {
			for (const auto& page : mod.pages) {
				for (const auto& control : page.controls) {
					if (control.identity.stableID != a_command.settingID)
						continue;
					if (found)
						return std::unexpected(BridgeError{ BridgeErrorCode::kStaleSnapshot, "Committed control identity is ambiguous" });
					found = &control;
				}
			}
		}
		if (!found || found->identity != a_command.expectedIdentity)
			return std::unexpected(BridgeError{ BridgeErrorCode::kStaleSnapshot, "Control identity changed during commit" });
		const bool textActivation = a_command.intent == WriteIntent::kActivate && a_command.expectedType == MCMControlType::kStepper && found->type == MCMControlType::kText;
		if (found->type != a_command.expectedType && !textActivation)
			return std::unexpected(BridgeError{ BridgeErrorCode::kStaleSnapshot, "Control type changed during commit" });
		if (std::holds_alternative<std::monostate>(found->value))
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Committed control has no confirmed value" });
		if (a_command.intent == WriteIntent::kSetValue && found->value != a_effectiveValue)
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "CloseConfig did not retain the effective requested value" });
		return found->value;
	}

	Result<std::reference_wrapper<const MCMControl>> ValidateWrite(
		const MCMSnapshot&  a_snapshot,
		const WriteCommand& a_command)
	{
		if (a_command.snapshotGeneration > a_snapshot.generation) {
			return std::unexpected(BridgeError{ BridgeErrorCode::kStaleSnapshot, "Snapshot generation is newer than the current state" });
		}
		const auto activationControl = a_command.expectedType == MCMControlType::kText ||
		                               a_command.expectedType == MCMControlType::kStepper;
		if (a_command.intent == WriteIntent::kActivate && !activationControl) {
			return std::unexpected(BridgeError{ BridgeErrorCode::kUnsupported, "Control does not support activation writes" });
		}
		if (a_command.intent == WriteIntent::kSetValue && activationControl) {
			return std::unexpected(BridgeError{ BridgeErrorCode::kUnsupported, "Control requires an activation write" });
		}

		for (const auto& mod : a_snapshot.mods) {
			for (const auto& page : mod.pages) {
				for (const auto& control : page.controls) {
					if (control.identity.stableID != a_command.settingID) {
						continue;
					}
					if (control.identity != a_command.expectedIdentity || control.type != a_command.expectedType) {
						return std::unexpected(BridgeError{ BridgeErrorCode::kStaleSnapshot, "Control identity or type changed" });
					}
					const auto resettable = a_command.intent == WriteIntent::kReset && !control.disabled && !control.hidden &&
					                        control.type != MCMControlType::kEmpty && control.type != MCMControlType::kHeader &&
					                        control.type != MCMControlType::kUnknown && control.writeCapability != WriteCapability::kUnsupported;
					if (control.writeCapability != WriteCapability::kWritable && !resettable) {
						return std::unexpected(BridgeError{ BridgeErrorCode::kUnsupported, "Control is not writable" });
					}
					if (control.value != a_command.expectedValue) {
						return std::unexpected(BridgeError{ BridgeErrorCode::kStaleSnapshot, "Control value changed" });
					}
					return std::cref(control);
				}
			}
		}

		return std::unexpected(BridgeError{ BridgeErrorCode::kNotFound, "Control was not found" });
	}
}
