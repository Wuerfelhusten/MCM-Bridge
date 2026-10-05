#pragma once

#include "MCMBridge/Core/Model.h"

namespace MCMBridge::InputSelector
{
	std::optional<std::string> Render(const MCMControl& a_control, bool a_enabled);
}
