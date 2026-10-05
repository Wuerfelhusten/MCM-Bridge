#pragma once

#include "MCMBridge/Core/Interfaces.h"
#include "MCMBridge/Papyrus/IMCMHostAdapter.h"

namespace MCMBridge
{
	struct LiveMCM
	{
		MCMDescriptor                    descriptor;
		std::int32_t                     configIndex{ -1 };
		std::shared_ptr<IMCMHostAdapter> adapter;
		std::string                      registryID;
		std::string                      registryDisplayName;
	};

	class ILiveMCMRegistryProvider : public IMCMRegistryProvider
	{
	public:
		virtual Result<std::vector<LiveMCM>> ReadLive() = 0;
		virtual bool                         IsAvailable() const = 0;
		virtual std::string_view             Name() const = 0;
	};
}
