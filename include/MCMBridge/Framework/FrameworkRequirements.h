#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace MCMBridge::FrameworkRequirements
{
	inline constexpr std::string_view minimumRelease = "3.18";
	inline constexpr std::uint32_t    minimumApiVersion = 1U;
	inline constexpr std::array       requiredExports{
		"GetMenuFrameworkAPIVersion",
		"GetMenuFrameworkVersion",
		"AddSectionItem",
		"RenameSection",
		"DeleteSection",
		"AddWindow",
		"GetMainWindow",
		"RegisterEventPriority",
		"UnregisterEvent",
		"RegisterInpoutEvent",
		"UnregisterInputEvent"
	};

	// Release 3.18 still reports file version 3.14 and legacy API version 3.8.
	// Neither number identifies the download release; require its host API instead.
	template <class HasExport>
	const char* FindMissingExport(const HasExport& a_hasExport)
	{
		for (const auto name : requiredExports) {
			if (!a_hasExport(name)) {
				return name;
			}
		}
		return nullptr;
	}

	constexpr bool SupportsApi(std::uint32_t a_version)
	{
		return a_version >= minimumApiVersion;
	}
}
