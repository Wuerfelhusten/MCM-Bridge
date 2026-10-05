#include "MCMBridge/Core/ClassicPageList.h"

#include <algorithm>

namespace MCMBridge
{
	bool IsSelectableClassicPageName(std::string_view a_name)
	{
		if (a_name.find_first_not_of(" \t\r\n\v\f") == std::string_view::npos) {
			return false;
		}
		constexpr std::string_view sentinel = "none";
		return !std::ranges::equal(a_name, sentinel, [](char a_left, char a_right) {
			const auto lower = a_left >= 'A' && a_left <= 'Z' ? a_left + ('a' - 'A') : a_left;
			return lower == a_right;
		});
	}

	std::vector<ClassicPageSelection> BuildClassicPageList(
		std::span<const std::string>        a_names,
		std::optional<ClassicPageSelection> a_openingPage)
	{
		std::vector<ClassicPageSelection> pages;
		if (a_openingPage && (a_openingPage->index == -1 || IsSelectableClassicPageName(a_openingPage->name))) {
			pages.push_back(*a_openingPage);
		}
		for (std::size_t index = 0; index < a_names.size(); ++index) {
			if (IsSelectableClassicPageName(a_names[index]) &&
				(!a_openingPage || static_cast<std::int32_t>(index) != a_openingPage->index)) {
				pages.push_back({ a_names[index], static_cast<std::int32_t>(index) });
			}
		}
		if (pages.empty()) {
			pages.push_back({ {}, -1 });
		}
		return pages;
	}
}
