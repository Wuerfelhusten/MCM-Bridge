#pragma once

namespace MCMBridge::GameSessionEvents
{
	bool Install();
	void BeginLoad();
	void GameStarted();
	// Called on the game task thread, never from a render callback.
	bool TryStartSession();
	bool CanUseGame();
}
