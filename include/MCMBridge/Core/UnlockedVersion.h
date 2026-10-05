#pragma once

#include <array>
#include <cstdint>

namespace MCMBridge
{
	constexpr bool SupportsUnlockedVersion(std::array<std::int32_t, 3> a_version)
	{
		return a_version[0] >= 0 && a_version[1] >= 0 && a_version[2] >= 0 &&
		       a_version >= std::array<std::int32_t, 3>{ 2, 1, 5 };
	}
}
