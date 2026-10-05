#include "MCMBridge/UI/ControlWrite.h"

#include "MCMBridge/Plugin/BridgeController.h"

namespace MCMBridge::ControlWrite
{
	bool IsEditable(const MCMSnapshot& a_snapshot, const MCMControl& a_control)
	{
		return !a_snapshot.refreshing && a_control.writeCapability == WriteCapability::kWritable &&
		       a_control.writeStatus != WriteStatus::kPending;
	}

	bool CanReset(const MCMSnapshot& a_snapshot, const MCMControl& a_control)
	{
		return !a_snapshot.refreshing && !a_control.disabled && !a_control.hidden &&
		       a_control.type != MCMControlType::kEmpty && a_control.type != MCMControlType::kHeader &&
		       a_control.type != MCMControlType::kUnknown && a_control.writeCapability != WriteCapability::kUnsupported &&
		       a_control.writeStatus != WriteStatus::kPending;
	}

	void Submit(const MCMSnapshot& a_snapshot, const MCMControl& a_control, MCMValue a_desiredValue)
	{
		if (!IsEditable(a_snapshot, a_control) || a_desiredValue == a_control.value) {
			return;
		}
		BridgeController::GetSingleton().Submit({ .snapshotGeneration = a_snapshot.generation,
			.settingID = a_control.identity.stableID,
			.expectedIdentity = a_control.identity,
			.expectedType = a_control.type,
			.expectedValue = a_control.value,
			.desiredValue = std::move(a_desiredValue) });
	}

	void Activate(const MCMSnapshot& a_snapshot, const MCMControl& a_control)
	{
		if (!IsEditable(a_snapshot, a_control) ||
			(a_control.type != MCMControlType::kText && a_control.type != MCMControlType::kStepper)) {
			return;
		}
		BridgeController::GetSingleton().Submit({ .snapshotGeneration = a_snapshot.generation,
			.settingID = a_control.identity.stableID,
			.expectedIdentity = a_control.identity,
			.expectedType = a_control.type,
			.expectedValue = a_control.value,
			.desiredValue = std::monostate{},
			.intent = WriteIntent::kActivate });
	}

	void Reset(const MCMSnapshot& a_snapshot, const MCMControl& a_control)
	{
		if (!CanReset(a_snapshot, a_control)) {
			return;
		}
		BridgeController::GetSingleton().Submit({ .snapshotGeneration = a_snapshot.generation,
			.settingID = a_control.identity.stableID,
			.expectedIdentity = a_control.identity,
			.expectedType = a_control.type,
			.expectedValue = a_control.value,
			.desiredValue = std::monostate{},
			.intent = WriteIntent::kReset });
	}
}
