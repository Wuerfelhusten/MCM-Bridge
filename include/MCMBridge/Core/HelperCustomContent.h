#pragma once

#include "MCMBridge/Core/Model.h"
#include "MCMBridge/Core/Result.h"

#include <span>

namespace MCMBridge
{
	// Borrowed verified release CustomContent layout; no foreign STL methods are called.
	Result<CustomContentMetadata> CopyHelperCustomContent(std::span<const std::byte> a_record);
}
