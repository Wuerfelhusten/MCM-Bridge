#pragma once

#include "MCMBridge/Core/Model.h"

#include <span>
#include <string_view>

namespace MCMBridge
{
	std::string MakeStableID(std::string_view a_prefix, std::span<const std::string_view> a_parts);

	std::string MakeClassicModID(std::string_view a_ownerPlugin, std::uint32_t a_questFormID, std::string_view a_scriptName);

	std::string MakeClassicPageID(std::string_view a_modID, std::string_view a_pageName, std::int32_t a_pageIndex);

	std::string MakeClassicControlID(
		std::string_view a_pageID,
		std::string_view a_stateName,
		std::uint16_t    a_optionIndex,
		MCMControlType   a_type,
		std::string_view a_label);

	std::string MakeHelperControlID(
		std::string_view a_ownerPlugin,
		std::uint32_t    a_questFormID,
		std::string_view a_scriptName,
		std::string_view a_pageName,
		std::string_view a_stateName,
		std::string_view a_controlID);
}
