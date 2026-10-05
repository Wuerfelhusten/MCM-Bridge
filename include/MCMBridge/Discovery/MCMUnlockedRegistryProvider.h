#pragma once

#include "MCMBridge/Core/UnlockedRegistryQuery.h"
#include "MCMBridge/Discovery/LiveMCM.h"

namespace MCMBridge
{
	class MCMUnlockedRegistryProvider final : public ILiveMCMRegistryProvider
	{
	public:
		Result<std::vector<MCMDescriptor>> Read() override;
		bool                               IsBusy() const override;
		Result<std::vector<LiveMCM>>       ReadLive() override;
		bool                               IsAvailable() const override;
		std::string_view                   Name() const override;
		void                               Reset();
		void                               SetCompletion(std::function<void()> a_completion) { completion = std::move(a_completion); }

	private:
		static RE::BSTSmartPointer<RE::BSScript::Object> ReadManager();
		std::shared_ptr<UnlockedRegistryQuery>           query;
		std::function<void()>                            completion;
	};
}
