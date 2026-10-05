#pragma once

#include "MCMBridge/Core/Model.h"
#include "MCMBridge/Core/Result.h"

#include <filesystem>
#include <optional>

namespace MCMBridge
{
	struct MCMHelperPaths
	{
		std::filesystem::path                config;
		std::optional<std::filesystem::path> defaults;
		std::optional<std::filesystem::path> userSettings;
	};

	class MCMHelperParser
	{
	public:
		Result<MCMMod> Parse(const MCMHelperPaths& a_paths) const;
	};
}
