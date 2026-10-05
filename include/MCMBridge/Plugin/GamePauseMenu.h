#pragma once

namespace MCMBridge::GamePauseMenu
{
	bool Install();
	// SetWanted is thread-safe; menu operations run on the game task thread.
	void SetWanted(bool a_wanted);
	void Synchronize();
	bool IsOpen();
	bool IsJournalOpen();
}
