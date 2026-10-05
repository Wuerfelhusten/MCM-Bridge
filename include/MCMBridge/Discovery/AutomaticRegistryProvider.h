#pragma once

#include "MCMBridge/Discovery/ClassicRegistryProvider.h"
#include "MCMBridge/Discovery/NativeRegistryProvider.h"

namespace MCMBridge
{
	class AutomaticRegistryProvider final : public ILiveMCMRegistryProvider
	{
	public:
		Result<std::vector<MCMDescriptor>> Read() override;
		bool                               IsBusy() const override;
		Result<std::vector<LiveMCM>>       ReadLive() override;
		bool                               IsAvailable() const override;
		std::string_view                   Name() const override;
		void                               Reset(bool a_invalidate = false);
		Result<bool>                       CheckCount(std::size_t a_expected);
		Result<bool>                       ActivateNative(RE::BSTSmartPointer<RE::BSScript::Object> a_manager, std::uint64_t a_session);
		NativeRegistryProvider&            Native() { return native; }
		bool                               UsesNative() const { return true; }
		Result<bool>                       PrepareNative(std::uint64_t a_session);

	private:
		void LogSelection(std::string_view a_name);

		ClassicRegistryProvider classic;
		NativeRegistryProvider  native;
		std::string             selectedName;
	};
}
