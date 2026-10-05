#include "MCMBridge/Core/PexIdentity.h"

#include <algorithm>
#include <optional>

namespace
{
	struct Sections
	{
		std::span<const std::uint8_t> strings;
		std::span<const std::uint8_t> body;
	};

	std::optional<Sections> ReadSections(std::span<const std::uint8_t> a_bytes)
	{
		constexpr std::uint8_t header[]{ 0xFA, 0x57, 0xC0, 0xDE, 3, 2, 0, 1 };
		if (a_bytes.size() < 16 || !std::equal(std::begin(header), std::end(header), a_bytes.begin()))
			return std::nullopt;
		std::size_t offset = 16;
		const auto  number = [&]() -> std::optional<std::size_t> {
			if (a_bytes.size() - offset < 2)
				return std::nullopt;
			const auto value = (static_cast<std::size_t>(a_bytes[offset]) << 8) | a_bytes[offset + 1];
			offset += 2;
			return value;
		};
		const auto text = [&] {
			const auto size = number();
			if (!size || *size > a_bytes.size() - offset)
				return false;
			offset += *size;
			return true;
		};
		for (int index = 0; index < 3; ++index)
			if (!text())
				return std::nullopt;
		const auto start = offset;
		const auto count = number();
		if (!count)
			return std::nullopt;
		for (std::size_t index = 0; index < *count; ++index)
			if (!text())
				return std::nullopt;
		if (offset == a_bytes.size() || a_bytes[offset] > 1)
			return std::nullopt;
		const bool debug = a_bytes[offset++] != 0;
		const auto strings = a_bytes.subspan(start, offset - start);
		if (debug) {
			if (a_bytes.size() - offset < 8)
				return std::nullopt;
			offset += 8;
		}
		if (offset == a_bytes.size())
			return std::nullopt;
		return Sections{ strings, a_bytes.subspan(offset) };
	}
}

namespace MCMBridge
{
	bool MatchesShippedPex(std::span<const std::uint8_t> a_actual, std::span<const std::uint8_t> a_expected)
	{
		const auto actual = ReadSections(a_actual);
		const auto expected = ReadSections(a_expected);
		return actual && expected && std::ranges::equal(actual->strings, expected->strings) &&
		       std::ranges::equal(actual->body, expected->body);
	}
}
