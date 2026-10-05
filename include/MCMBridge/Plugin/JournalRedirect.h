#pragma once

namespace RE
{
	class GFxMovie;
}

namespace MCMBridge::JournalRedirect
{
	bool Install();
	// Invoke synchronously on the movie's UI thread, bypassing only this call.
	bool OpenOriginalPanel(RE::GFxMovie& a_movie);
}
