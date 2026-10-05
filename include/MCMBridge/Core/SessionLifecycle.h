#pragma once

#include <cstdint>

namespace MCMBridge
{
	// The owner serializes menu events, load notifications, and game-thread checks.
	class SessionLifecycle
	{
	public:
		std::uint64_t ObserveMainMenu(bool a_opening);
		void          BeginLoad();
		void          GameStarted();
		bool          IsCurrentMainMenuEvent(std::uint64_t a_token, bool a_mainMenuOpen) const;
		bool          CanUseGame(bool a_playerInCell, bool a_blockingMenuOpen) const;
		bool          TryActivate(bool a_playerInCell, bool a_blockingMenuOpen);
		bool          NeedsActivation() const;

	private:
		std::uint64_t revision{};
		bool          loading{};
		bool          needsActivation{ true };
	};
}
