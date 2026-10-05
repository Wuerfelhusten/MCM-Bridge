#pragma once

#include "MCMBridge/Core/Result.h"

#include <nlohmann/json_fwd.hpp>

namespace MCMBridge
{
	Result<void> ValidateMCMHelperConfig(const nlohmann::json& a_document);
}
