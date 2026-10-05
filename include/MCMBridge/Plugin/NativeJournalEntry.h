#pragma once

namespace RE
{
	class GFxMovieView;
	class GFxValue;
}

namespace MCMBridge
{
	// Called on the movie thread; the movie owns the installed functions.
	bool AttachNativeJournalEntry(RE::GFxMovieView& a_movie, RE::GFxValue& a_root, const RE::GFxValue& a_open);
}
