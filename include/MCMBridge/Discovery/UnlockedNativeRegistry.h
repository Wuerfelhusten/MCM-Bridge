#pragma once
#include "MCMBridge/Core/UnlockedRegistryQuery.h"

namespace MCMBridge
{
	std::shared_ptr<UnlockedRegistryQuery> CreateUnlockedRegistryQuery();
	void                                   ReadUnlockedRegistryCount(std::function<void(Result<std::int32_t>)> a_done);
}
