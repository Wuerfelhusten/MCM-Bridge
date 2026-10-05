#pragma once

#include <cstdint>
#include <span>

namespace MCMBridge
{
	struct LiveMCM;
	void RunNativeRegistryPreflight(std::span<const LiveMCM> a_entries, std::uint64_t a_session);
	// Game task only. Temporary allocation checks do not establish save or Helper compatibility.
	void RunNativeHostPreflight();
}
