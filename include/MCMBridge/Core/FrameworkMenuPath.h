#pragma once

#include <string>
#include <string_view>
#include <unordered_set>

namespace MCMBridge
{
	std::string                       EscapeFrameworkMenuPathSegment(std::string_view a_segment);
	std::string                       UniqueFrameworkMenuLabel(std::string_view a_label, std::unordered_set<std::string>& a_used);
	inline constexpr std::string_view mcmFolderPath = "MCMs";
	std::string                       MCMFrameworkSectionPath(std::string_view a_sectionSegment, bool a_grouped, std::string_view a_range = {});
	std::string                       JoinFrameworkMenuPath(std::string_view a_sectionSegment, std::string_view a_pageSegment, bool a_grouped = false, std::string_view a_range = {});
}
