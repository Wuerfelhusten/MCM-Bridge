#include "MCMBridge/Core/HelperMenuCapture.h"

#include <cmath>
#include <cstring>
#include <limits>

namespace MCMBridge
{
	std::optional<std::int32_t> ConvertMenuDialogIndex(double a_value)
	{
		if (!std::isfinite(a_value) || a_value < -1 || a_value > std::numeric_limits<std::int32_t>::max() || std::trunc(a_value) != a_value)
			return std::nullopt;
		return static_cast<std::int32_t>(a_value);
	}

	std::uint64_t HelperCodeFingerprint(std::span<const std::byte> a_code)
	{
		std::uint64_t hash = 14695981039346656037ULL;
		for (const auto value : a_code)
			hash = (hash ^ std::to_integer<unsigned char>(value)) * 1099511628211ULL;
		return hash;
	}

	Result<std::vector<std::string>> CopyHelperMenuStrings(std::span<const std::byte> a_records)
	{
		constexpr std::size_t stride = 32;
		if (a_records.size() % stride)
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Helper menu string layout is incomplete" });
		std::vector<std::string> result;
		result.reserve(a_records.size() / stride);
		for (std::size_t offset = 0; offset < a_records.size(); offset += stride) {
			const auto*   record = a_records.data() + offset;
			std::uint64_t size{}, capacity{};
			std::memcpy(&size, record + 16, sizeof(size));
			std::memcpy(&capacity, record + 24, sizeof(capacity));
			if (size > capacity || size > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max()))
				return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Helper menu string length is invalid" });
			const char* data = reinterpret_cast<const char*>(record);
			if (capacity > 15)
				std::memcpy(&data, record, sizeof(data));
			if (!data)
				return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Helper menu string storage is missing" });
			result.emplace_back(data, static_cast<std::size_t>(size));
		}
		return result;
	}
}
