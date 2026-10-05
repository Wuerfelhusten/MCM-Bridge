#pragma once

#include "MCMBridge/Discovery/LiveMCM.h"

#include "RE/Skyrim.h"

namespace MCMBridge
{
	class ClassicRegistryProvider final : public ILiveMCMRegistryProvider
	{
	public:
		Result<std::vector<MCMDescriptor>> Read() override;
		bool                               IsBusy() const override;

		Result<std::vector<LiveMCM>> ReadLive() override;
		bool                         IsAvailable() const override;
		std::string_view             Name() const override;

		// Shared bootstrap lookup; this does not open or scan a configuration.
		RE::BSTSmartPointer<RE::BSScript::Object> ReadManager() const;
	};
}
