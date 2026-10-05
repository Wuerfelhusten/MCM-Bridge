#include "MCMBridge/Core/FrameworkMenuPath.h"

#include <format>

namespace MCMBridge
{
	std::string EscapeFrameworkMenuPathSegment(std::string_view a_segment)
	{
		std::string result;
		result.reserve(a_segment.size());
		for (const auto character : a_segment) {
			if (character == '/') {
				result.push_back('\\');
			}
			result.push_back(character);
		}
		return result;
	}

	std::string UniqueFrameworkMenuLabel(std::string_view a_label, std::unordered_set<std::string>& a_used)
	{
		std::string candidate(a_label);
		for (std::size_t suffix = 2; !a_used.insert(candidate).second; ++suffix)
			candidate = std::format("{} ({})", a_label, suffix);
		return candidate;
	}

	std::string MCMFrameworkSectionPath(std::string_view a_sectionSegment, bool a_grouped, std::string_view a_range)
	{
		const auto section = EscapeFrameworkMenuPathSegment(a_sectionSegment);
		if (a_grouped && !a_range.empty())
			return std::format("{}/{}/{}", mcmFolderPath, EscapeFrameworkMenuPathSegment(a_range), section);
		return a_grouped ? std::format("{}/{}", mcmFolderPath, section) : section;
	}

	std::string JoinFrameworkMenuPath(std::string_view a_sectionSegment, std::string_view a_pageSegment, bool a_grouped, std::string_view a_range)
	{
		return std::format(
			"{}/{}", MCMFrameworkSectionPath(a_sectionSegment, a_grouped, a_range), EscapeFrameworkMenuPathSegment(a_pageSegment));
	}
}
