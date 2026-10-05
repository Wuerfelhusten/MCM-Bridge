#pragma once

#include "MCMBridge/Core/Result.h"

#include <cstdint>
#include <istream>
#include <string_view>

namespace MCMBridge
{
	Result<bool>             IsPluginActive(std::istream& a_input, std::string_view a_plugin);
	Result<std::string_view> PluginListFolder(std::uint32_t a_packedRuntime);
}
