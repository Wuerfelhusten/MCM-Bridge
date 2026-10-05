#pragma once

#include "MCMBridge/Core/Result.h"
#include <cstdint>
#include <string>
#include <string_view>

namespace MCMBridge
{
	Result<bool> IsRecorderEnabledAtStartup(std::uint32_t a_packedRuntime);
	Result<bool> IsUnlockedSupportedAtStartup();
	std::string  FindIncompatibleHostScripts();
	void         ShowStartupIncompatibility(bool a_redone, bool a_recorder, bool a_seeded, bool a_unlocked = false, bool a_menuMaid = false, std::string_view a_scripts = {});
}
