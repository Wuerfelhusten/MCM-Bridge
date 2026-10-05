#pragma once

#include <algorithm>
#include <array>
#include <span>
#include <string_view>

namespace MCMBridge
{
	inline bool IsVanillaJournalCatalog(std::span<const std::string_view> a_labels)
	{
		constexpr std::array<std::string_view, 7> se{
			"$QUICKSAVE", "$SAVE", "$LOAD", "$SETTINGS", "$CONTROLS", "$HELP", "$QUIT"
		};
		constexpr std::array<std::string_view, 8> ae{
			"$QUICKSAVE", "$SAVE", "$LOAD", "$INSTALLED CONTENT", "$SETTINGS", "$CONTROLS", "$HELP", "$QUIT"
		};
		constexpr std::array<std::string_view, 9> creations{
			"$QUICKSAVE", "$SAVE", "$LOAD", "$INSTALLED CONTENT", "$CREATIONS", "$SETTINGS", "$CONTROLS", "$HELP", "$QUIT"
		};
		return std::ranges::equal(a_labels, se) || std::ranges::equal(a_labels, ae) ||
		       std::ranges::equal(a_labels, creations);
	}
}
