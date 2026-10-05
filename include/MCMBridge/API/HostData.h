#pragma once

#include "MCMBridge/API/MCMBridgeHost.h"
#include "MCMBridge/Core/Interfaces.h"

namespace MCMBridge
{
	struct HostDataInput
	{
		std::uint64_t               session{};
		std::vector<MCMDescriptor>  registry;
		std::optional<MCMMod>       active;
		std::optional<std::int32_t> currentPage;
	};

	// Owns every string and nested view until the client releases the opaque handle.
	class HostData
	{
	public:
		explicit HostData(HostDataInput a_input);
		HostData(const HostData&) = delete;
		HostData&       operator=(const HostData&) = delete;
		MCMHostDataView View() const;

	private:
		HostDataInput                                      input;
		std::vector<MCMHostModView>                        mods;
		std::vector<MCMHostPageView>                       pages;
		std::vector<std::vector<MCMHostControlView>>       controls;
		std::vector<std::vector<std::vector<const char*>>> menus;
	};

	std::uint32_t   HostControlType(MCMControlType a_type);
	MCMHostArgument HostValue(const MCMValue& a_value);
	const char*     HostModName(std::string_view a_identity, const std::string& a_fallback);
}
