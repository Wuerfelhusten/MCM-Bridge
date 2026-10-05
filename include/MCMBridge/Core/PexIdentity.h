#pragma once

#include <cstdint>
#include <span>

namespace MCMBridge
{
	// Compare shipped code, ignoring compiler timestamps and build-machine header strings.
	bool MatchesShippedPex(std::span<const std::uint8_t> a_actual, std::span<const std::uint8_t> a_expected);
}
