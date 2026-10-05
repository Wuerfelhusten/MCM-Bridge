#pragma once

#include <span>
#include <string>
#include <string_view>

namespace MCMBridge
{
	bool        ValidMCMRanges(std::string_view a_ends);
	std::string MCMNameSortKey(std::string_view a_name);
	std::string UppercaseMCMRootInitial(std::string_view a_name);
	std::string MCMNameRange(std::string_view a_name, std::string_view a_ends);
	std::string BalanceMCMRanges(std::span<const std::string> a_names, int a_groups);
}
