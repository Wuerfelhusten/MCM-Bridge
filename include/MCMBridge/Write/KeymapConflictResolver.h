#pragma once

#include "MCMBridge/Core/Model.h"

#include <utility>

namespace MCMBridge
{
	std::pair<std::string, std::string> ResolveKeymapConflict(
		const MCMSnapshot& a_snapshot,
		const MCMControl&  a_control,
		std::int32_t       a_keyCode);
}
