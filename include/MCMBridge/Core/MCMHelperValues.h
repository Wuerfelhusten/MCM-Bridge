#pragma once

#include "MCMBridge/Core/Model.h"

#include <filesystem>
#include <nlohmann/json_fwd.hpp>
#include <unordered_map>

namespace MCMBridge
{
	using MCMHelperSettings = std::unordered_map<std::string, MCMValue>;

	void ReadMCMHelperIni(const std::filesystem::path& a_path, MCMHelperSettings& a_settings);
	void ApplyMCMHelperValueOptions(
		MCMControl&              a_control,
		const nlohmann::json&    a_valueOptions,
		const MCMHelperSettings& a_defaults,
		const MCMHelperSettings& a_current);
}
