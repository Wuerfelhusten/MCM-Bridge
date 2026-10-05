#include "MCMBridge/Write/WritePauseState.h"

namespace MCMBridge
{
	std::uint64_t WritePauseState::Reserve()
	{
		const auto ticket = nextTicket++;
		pending.insert(ticket);
		return ticket;
	}

	bool WritePauseState::Begin(std::uint64_t a_ticket)
	{
		if (!Contains(a_ticket))
			return false;
		batchStarted = true;
		return true;
	}

	bool WritePauseState::Complete(std::uint64_t a_ticket)
	{
		const auto removed = pending.erase(a_ticket) != 0;
		if (pending.empty())
			openingSince.reset();
		return removed;
	}

	Result<bool> WritePauseState::AwaitPause(
		std::uint64_t a_ticket, bool a_menuOpen, std::chrono::steady_clock::time_point a_now)
	{
		if (!Contains(a_ticket)) {
			return std::unexpected(BridgeError{ BridgeErrorCode::kStaleSnapshot, "Setting change was cancelled" });
		}
		Begin(a_ticket);
		if (!ShouldPause() || a_menuOpen) {
			openingSince.reset();
			return true;
		}
		if (!openingSince)
			openingSince = a_now;
		if (a_now - *openingSince >= std::chrono::seconds(2)) {
			return std::unexpected(BridgeError{ BridgeErrorCode::kTimedOut, "Engine pause did not activate; no setting callback was started" });
		}
		return false;
	}

	void WritePauseState::SetEnabled(bool a_enabled)
	{
		enabled = a_enabled;
		if (!a_enabled)
			openingSince.reset();
	}
	void WritePauseState::MenuClosed()
	{
		if (pending.empty())
			batchStarted = false;
	}
	void WritePauseState::Reset()
	{
		pending.clear();
		batchStarted = false;
		openingSince.reset();
	}
	bool        WritePauseState::Contains(std::uint64_t a_ticket) const { return pending.contains(a_ticket); }
	bool        WritePauseState::Enabled() const { return enabled; }
	bool        WritePauseState::ShouldPause() const { return enabled && batchStarted && !pending.empty(); }
	std::size_t WritePauseState::Pending() const { return pending.size(); }
}
