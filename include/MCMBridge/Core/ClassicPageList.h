#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace MCMBridge
{
	struct ClassicPageSelection
	{
		std::string  name;
		std::int32_t index{ -1 };

		bool operator==(const ClassicPageSelection&) const = default;
	};

	bool IsSelectableClassicPageName(std::string_view a_name);

	// Preserve Papyrus array indices and raw keys when omitting navigation placeholders.
	std::vector<ClassicPageSelection> BuildClassicPageList(
		std::span<const std::string>        a_names,
		std::optional<ClassicPageSelection> a_openingPage = std::nullopt);
}
