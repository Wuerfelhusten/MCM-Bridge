#pragma once

#include "MCMBridge/Papyrus/IClassicScript.h"

#include <memory>

namespace MCMBridge
{
	// Creates game-thread sessions implementing the MCM callback protocol.
	// The adapter owns host-specific handles; consumers never construct them.
	class IMCMHostAdapter
	{
	public:
		virtual ~IMCMHostAdapter() = default;
		virtual bool                            IsValid() const { return true; }
		virtual std::shared_ptr<IClassicScript> CreateSession() const = 0;
		// Game-thread read of registered navigation only. Must not acquire a host
		// session, dispatch callbacks or imply that page contents are current.
		virtual std::optional<std::vector<std::string>> ReadRegisteredPages() const { return std::nullopt; }
	};
}
