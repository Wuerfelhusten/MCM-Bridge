#include "MCMBridge/Plugin/WritePauseService.h"

#include "MCMBridge/Plugin/BridgeSettingsService.h"
#include "MCMBridge/Plugin/GamePauseMenu.h"

namespace MCMBridge
{
	WritePauseService& WritePauseService::GetSingleton()
	{
		static WritePauseService service;
		return service;
	}

	void WritePauseService::LoadSettings()
	{
		const auto             settings = BridgeSettingsService::GetSingleton().Get();
		const std::scoped_lock lock(mutex);
		state.SetEnabled(settings.pauseDuringWrites);
		SKSE::log::info("Pause during setting changes: {}", state.Enabled());
	}

	std::uint64_t WritePauseService::Reserve()
	{
		const std::scoped_lock lock(mutex);
		const auto             ticket = state.Reserve();
		UpdateWanted();
		return ticket;
	}

	Result<bool> WritePauseService::Begin(std::uint64_t a_ticket)
	{
		const auto             menuOpen = GamePauseMenu::IsOpen();
		const std::scoped_lock lock(mutex);
		const auto             result = state.AwaitPause(a_ticket, menuOpen, std::chrono::steady_clock::now());
		UpdateWanted();
		return result;
	}

	void WritePauseService::Complete(std::uint64_t a_ticket)
	{
		const auto             menuOpen = GamePauseMenu::IsOpen();
		const std::scoped_lock lock(mutex);
		state.Complete(a_ticket);
		if (!menuOpen)
			state.MenuClosed();
		UpdateWanted();
	}

	void WritePauseService::Reset()
	{
		const std::scoped_lock lock(mutex);
		state.Reset();
		UpdateWanted();
	}

	void WritePauseService::MenuClosed()
	{
		const std::scoped_lock lock(mutex);
		state.MenuClosed();
		UpdateWanted();
	}

	void WritePauseService::Reconcile()
	{
		const std::scoped_lock lock(mutex);
		UpdateWanted();
	}

	bool WritePauseService::Enabled() const
	{
		const std::scoped_lock lock(mutex);
		return state.Enabled();
	}

	std::size_t WritePauseService::Pending() const
	{
		const std::scoped_lock lock(mutex);
		return state.Pending();
	}

	void WritePauseService::SetEnabled(bool a_enabled)
	{
		{
			const std::scoped_lock lock(mutex);
			state.SetEnabled(a_enabled);
			UpdateWanted();
		}
		BridgeSettingsService::GetSingleton().SetPauseDuringWrites(a_enabled);
		SKSE::log::info("Pause during setting changes: {}", a_enabled);
	}

	void WritePauseService::UpdateWanted()
	{
		GamePauseMenu::SetWanted(state.ShouldPause());
		QueueSynchronize();
	}

	void WritePauseService::QueueSynchronize()
	{
		if (syncQueued.exchange(true))
			return;
		if (auto* tasks = SKSE::GetTaskInterface()) {
			tasks->AddTask([] {
				GetSingleton().syncQueued.store(false);
				GamePauseMenu::Synchronize();
			});
		} else {
			syncQueued.store(false);
		}
	}
}
