#pragma once

#include <string_view>

namespace MCMBridge
{
	// Papyrus identifiers are case-insensitive, including names interned by the VM.
	constexpr bool SamePapyrusIdentifier(std::string_view a_left, std::string_view a_right)
	{
		if (a_left.size() != a_right.size())
			return false;
		const auto fold = [](char a_character) {
			return a_character >= 'A' && a_character <= 'Z' ? static_cast<char>(a_character + ('a' - 'A')) : a_character;
		};
		for (std::size_t index = 0; index < a_left.size(); ++index)
			if (fold(a_left[index]) != fold(a_right[index]))
				return false;
		return true;
	}
}
