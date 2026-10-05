#pragma once

#include "MCMBridge/Core/Model.h"

namespace MCMBridge::ControlRenderer
{
	const char* BackendName(MCMBackendKind a_backend);
	void        Render(const MCMSnapshot& a_snapshot, const MCMControl& a_control);
}
