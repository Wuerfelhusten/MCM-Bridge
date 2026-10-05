#pragma once

#include "MCMBridge/Core/Result.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace MCMBridge
{
	// Verified Helper 1.4.0/1.5.0/1.6.2/1.6.3 Windows x64 release string records. Borrowed heap
	// pointers remain valid only during its setter call; copy before returning.
	Result<std::vector<std::string>> CopyHelperMenuStrings(std::span<const std::byte> a_records);
	std::uint64_t                    HelperCodeFingerprint(std::span<const std::byte> a_code);
	std::optional<std::int32_t>      ConvertMenuDialogIndex(double a_value);
}
