#pragma once

#include "MCMBridge/Core/Result.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_set>

namespace MCMBridge
{
	// The owner serializes access. Tickets survive queue moves and expire only once.
	class WritePauseState
	{
	public:
		std::uint64_t Reserve();
		bool          Begin(std::uint64_t a_ticket);
		Result<bool>  AwaitPause(std::uint64_t a_ticket, bool a_menuOpen, std::chrono::steady_clock::time_point a_now);
		bool          Complete(std::uint64_t a_ticket);
		void          SetEnabled(bool a_enabled);
		void          MenuClosed();
		void          Reset();
		bool          Contains(std::uint64_t a_ticket) const;
		bool          Enabled() const;
		bool          ShouldPause() const;
		std::size_t   Pending() const;

	private:
		std::unordered_set<std::uint64_t>                    pending;
		std::uint64_t                                        nextTicket{ 1 };
		bool                                                 enabled{ true };
		bool                                                 batchStarted{};
		std::optional<std::chrono::steady_clock::time_point> openingSince;
	};
}
