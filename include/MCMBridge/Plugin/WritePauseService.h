#pragma once

#include "MCMBridge/Core/Result.h"
#include "MCMBridge/Write/WritePauseState.h"

#include <atomic>
#include <mutex>

namespace MCMBridge
{
	class WritePauseService
	{
	public:
		static WritePauseService& GetSingleton();
		void                      LoadSettings();
		std::uint64_t             Reserve();
		// Begin and Complete run on the game task thread.
		Result<bool> Begin(std::uint64_t a_ticket);
		void         Complete(std::uint64_t a_ticket);
		void         Reset();
		void         MenuClosed();
		void         Reconcile();
		bool         Enabled() const;
		std::size_t  Pending() const;
		void         SetEnabled(bool a_enabled);

	private:
		void               UpdateWanted();
		void               QueueSynchronize();
		mutable std::mutex mutex;
		WritePauseState    state;
		std::atomic_bool   syncQueued{};
	};
}
