#include "MCMBridge/Core/MCMOrganization.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace MCMBridge
{
	bool ValidMCMRanges(std::string_view a_ends)
	{
		char previous = 'A' - 1;
		for (const char end : a_ends) {
			if (end <= previous || end > 'Z')
				return false;
			previous = end;
		}
		return !a_ends.empty() && a_ends.back() == 'Z';
	}

	std::string MCMNameSortKey(std::string_view a_name)
	{
		std::string result;
		for (std::size_t i = 0; i < a_name.size(); ++i) {
			auto ch = static_cast<unsigned char>(a_name[i]);
			// Fold common German letters without depending on the process locale.
			if (ch == 0xC3 && i + 1 < a_name.size()) {
				const auto next = static_cast<unsigned char>(a_name[i + 1]);
				if (next == 0x84 || next == 0xA4)
					ch = 'A';
				else if (next == 0x96 || next == 0xB6)
					ch = 'O';
				else if (next == 0x9C || next == 0xBC)
					ch = 'U';
				else if (next == 0x9F)
					ch = 'S';
				if (ch != 0xC3)
					++i;
			}
			if (ch >= 'a' && ch <= 'z')
				ch -= 'a' - 'A';
			result.push_back(static_cast<char>(ch));
		}
		const auto first = result.find_first_not_of(" \t\r\n");
		return first == std::string::npos ? std::string{} : result.substr(first);
	}

	std::string UppercaseMCMRootInitial(std::string_view a_name)
	{
		std::string result(a_name);
		const auto  first = result.find_first_not_of(" \t\r\n");
		if (first == std::string::npos)
			return result;
		auto& initial = result[first];
		if (initial >= 'a' && initial <= 'z') {
			initial -= 'a' - 'A';
		} else if (static_cast<unsigned char>(initial) == 0xC3 && first + 1 < result.size()) {
			auto& next = result[first + 1];
			if (const auto letter = static_cast<unsigned char>(next); letter == 0xA4 || letter == 0xB6 || letter == 0xBC)
				next = static_cast<char>(letter - 0x20);
			else if (letter == 0x9F)
				result.replace(first, 2, "\xE1\xBA\x9E");
		}
		return result;
	}

	std::string MCMNameRange(std::string_view a_name, std::string_view a_ends)
	{
		const auto key = MCMNameSortKey(a_name);
		if (key.empty() || key.front() < 'A' || key.front() > 'Z')
			return "0-9 & Other";
		if (!ValidMCMRanges(a_ends))
			a_ends = "Z";
		char start = 'A';
		for (const char end : a_ends) {
			if (key.front() <= end)
				return start == end ? std::string(1, start) : std::string(1, start) + "-" + end;
			start = static_cast<char>(end + 1);
		}
		return "0-9 & Other";
	}

	std::string BalanceMCMRanges(std::span<const std::string> a_names, int a_groups)
	{
		const int           groups = std::clamp(a_groups, 1, 26);
		std::array<int, 27> prefix{};
		for (const auto& name : a_names) {
			const auto key = MCMNameSortKey(name);
			if (!key.empty() && key.front() >= 'A' && key.front() <= 'Z')
				++prefix[static_cast<std::size_t>(key.front() - 'A' + 1)];
		}
		for (std::size_t i = 1; i < prefix.size(); ++i) prefix[i] += prefix[i - 1];
		// Partition whole letters, minimizing population imbalance. Empty lists
		// use equal letter spans; no setting is changed until the user applies it.
		std::array<std::array<double, 27>, 27> cost;
		std::array<std::array<int, 27>, 27>    previous{};
		for (auto& row : cost) row.fill(std::numeric_limits<double>::infinity());
		cost[0][0] = 0;
		const auto target = static_cast<double>(prefix[26] ? prefix[26] : 26) / groups;
		for (int group = 1; group <= groups; ++group) {
			for (int end = group; end <= 26; ++end) {
				for (int start = group - 1; start < end; ++start) {
					const double size = prefix[26] ? prefix[end] - prefix[start] : end - start;
					const auto   candidate = cost[group - 1][start] + (size - target) * (size - target);
					if (candidate < cost[group][end]) {
						cost[group][end] = candidate;
						previous[group][end] = start;
					}
				}
			}
		}
		std::string ends(static_cast<std::size_t>(groups), 'Z');
		int         end = 26;
		for (int group = groups; group > 0; --group) {
			ends[static_cast<std::size_t>(group - 1)] = static_cast<char>('A' + end - 1);
			end = previous[group][end];
		}
		return ends;
	}
}
