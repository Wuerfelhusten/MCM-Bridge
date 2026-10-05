#pragma once

#include "MCMBridge/Core/Model.h"

namespace MCMBridge::ControlWrite
{
	bool IsEditable(const MCMSnapshot& a_snapshot, const MCMControl& a_control);
	bool CanReset(const MCMSnapshot& a_snapshot, const MCMControl& a_control);
	void Submit(const MCMSnapshot& a_snapshot, const MCMControl& a_control, MCMValue a_desiredValue);
	void Activate(const MCMSnapshot& a_snapshot, const MCMControl& a_control);
	void Reset(const MCMSnapshot& a_snapshot, const MCMControl& a_control);
}
