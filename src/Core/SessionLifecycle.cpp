#include "MCMBridge/Core/SessionLifecycle.h"

namespace MCMBridge
{
	std::uint64_t SessionLifecycle::ObserveMainMenu(bool a_opening)
	{
		if (a_opening)
			needsActivation = true;
		return ++revision;
	}

	void SessionLifecycle::BeginLoad()
	{
		++revision;
		loading = true;
		needsActivation = true;
	}

	void SessionLifecycle::GameStarted()
	{
		++revision;
		loading = false;
		needsActivation = false;
	}

	bool SessionLifecycle::IsCurrentMainMenuEvent(std::uint64_t a_token, bool a_mainMenuOpen) const
	{
		return a_token == revision && a_mainMenuOpen && !loading;
	}

	bool SessionLifecycle::CanUseGame(bool a_playerInCell, bool a_blockingMenuOpen) const
	{
		return !loading && a_playerInCell && !a_blockingMenuOpen;
	}

	bool SessionLifecycle::TryActivate(bool a_playerInCell, bool a_blockingMenuOpen)
	{
		if (!needsActivation || !CanUseGame(a_playerInCell, a_blockingMenuOpen))
			return false;
		GameStarted();
		return true;
	}

	bool SessionLifecycle::NeedsActivation() const { return needsActivation; }
}
