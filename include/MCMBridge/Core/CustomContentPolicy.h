#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace MCMBridge
{
	enum class CustomContentOrigin
	{
		kUser,
		kScan,
		kRestore
	};

	struct CustomContentVisits
	{
		std::string   modID;
		std::string   pageID;
		std::uint64_t count{};
		std::uint64_t scanResults{};
		std::uint64_t restoreResults{};
		bool          Observe(std::string_view a_modID, std::string_view a_pageID, CustomContentOrigin a_origin = CustomContentOrigin::kUser)
		{
			// Background results never change the currently displayed user visit.
			if (a_origin != CustomContentOrigin::kUser) {
				++(a_origin == CustomContentOrigin::kScan ? scanResults : restoreResults);
				return true;
			}
			if (modID == a_modID && pageID == a_pageID)
				return false;
			modID = a_modID;
			pageID = a_pageID;
			++count;
			return true;
		}
		void Leave()
		{
			modID.clear();
			pageID.clear();
		}
	};

	inline std::string_view CustomContentPlaceholder(std::string_view a_source)
	{
		if (a_source.size() >= 4) {
			const auto suffix = a_source.substr(a_source.size() - 4);
			if (suffix[0] == '.' && (suffix[1] == 'd' || suffix[1] == 'D') &&
				(suffix[2] == 'd' || suffix[2] == 'D') && (suffix[3] == 's' || suffix[3] == 'S'))
				return "Missing DDS Source";
		}
		return "Missing SWF Source";
	}
}
