#include "MCMBridge/Core/StableId.h"

#include <array>
#include <format>

namespace
{
	std::uint64_t HashPart(std::uint64_t a_hash, std::string_view a_part)
	{
		constexpr std::uint64_t prime = 1099511628211ULL;
		for (const unsigned char value : a_part) {
			a_hash ^= value;
			a_hash *= prime;
		}
		a_hash ^= 0xFFU;
		a_hash *= prime;
		return a_hash;
	}
}

namespace MCMBridge
{
	std::string MakeStableID(std::string_view a_prefix, std::span<const std::string_view> a_parts)
	{
		std::uint64_t hash = 14695981039346656037ULL;
		for (const auto part : a_parts) {
			hash = HashPart(hash, part);
		}
		return std::format("{}:{:016x}", a_prefix, hash);
	}

	std::string MakeClassicModID(std::string_view a_ownerPlugin, std::uint32_t a_questFormID, std::string_view a_scriptName)
	{
		const auto       formID = std::format("{:08x}", a_questFormID);
		const std::array parts{ a_ownerPlugin, std::string_view(formID), a_scriptName };
		return MakeStableID("classic-mod", parts);
	}

	std::string MakeClassicPageID(std::string_view a_modID, std::string_view a_pageName, std::int32_t a_pageIndex)
	{
		const auto       index = std::to_string(a_pageIndex);
		const std::array parts{ a_modID, a_pageName, std::string_view(index) };
		return MakeStableID("classic-page", parts);
	}

	std::string MakeClassicControlID(
		std::string_view a_pageID,
		std::string_view a_stateName,
		std::uint16_t    a_optionIndex,
		MCMControlType   a_type,
		std::string_view a_label)
	{
		const auto       index = std::to_string(a_optionIndex);
		const auto       typeValue = std::to_string(static_cast<int>(a_type));
		const std::array parts{ a_pageID, a_stateName, std::string_view(index), std::string_view(typeValue), a_label };
		return MakeStableID("classic-control", parts);
	}

	std::string MakeHelperControlID(
		std::string_view a_ownerPlugin,
		std::uint32_t    a_questFormID,
		std::string_view a_scriptName,
		std::string_view a_pageName,
		std::string_view a_stateName,
		std::string_view a_controlID)
	{
		const auto       formID = std::format("{:08x}", a_questFormID);
		const std::array parts{ a_ownerPlugin, std::string_view(formID), a_scriptName, a_pageName, a_stateName, a_controlID };
		return MakeStableID("helper-control", parts);
	}
}
