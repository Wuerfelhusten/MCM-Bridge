#pragma once

#include "MCMBridge/Core/Model.h"
#include "MCMBridge/Core/Result.h"

#include <cstdint>
#include <string>
#include <vector>

namespace MCMBridge
{
	struct ClassicPageContext
	{
		std::string   modID;
		std::string   ownerPlugin;
		std::uint32_t questFormID{};
		std::string   scriptName;
		std::string   pageName;
		std::int32_t  pageIndex{};
	};

	struct ClassicPageBuffers
	{
		std::vector<std::int32_t> optionFlags;
		std::vector<std::string>  labels;
		std::vector<std::string>  stringValues;
		std::vector<float>        numericValues;
		std::vector<std::string>  stateNames;
	};

	Result<MCMPage> ParseClassicPage(const ClassicPageContext& a_context, const ClassicPageBuffers& a_buffers);
}
