#pragma once

#include "MCMBridge/Core/BridgeSettings.h"

#include <cstdint>

namespace MCMBridge
{
	enum class JournalRedirectAction
	{
		kUnavailable,
		kKeepJournal,
		kCloseJournal
	};
	class JournalHandoff
	{
	public:
		std::uint64_t Begin(std::uint64_t a_generation)
		{
			if (request || nextRequest == 0)
				return 0;
			generation = a_generation;
			closed = false;
			return request = nextRequest++;
		}

		std::uint64_t JournalClosed(std::uint64_t a_generation)
		{
			if (!request || closed || a_generation != generation + 1)
				return 0;
			closed = true;
			return request;
		}

		bool Complete(std::uint64_t a_request, std::uint64_t a_generation, bool a_journalOpen, bool a_unavailable)
		{
			if (!a_request || a_request != request || !closed)
				return false;
			request = 0;
			return a_generation == generation + 1 && !a_journalOpen && !a_unavailable;
		}

		bool Expire(std::uint64_t a_request)
		{
			if (!a_request || a_request != request || closed)
				return false;
			request = 0;
			return true;
		}

		void Cancel() { request = 0; }

	private:
		std::uint64_t nextRequest{ 1 };
		std::uint64_t request{};
		std::uint64_t generation{};
		bool          closed{};
	};

	constexpr JournalRedirectAction ResolveJournalRedirect(
		const BridgeSettings& a_settings, bool a_frameworkAvailable)
	{
		if (!a_frameworkAvailable)
			return JournalRedirectAction::kUnavailable;
		return a_settings.closeJournalOnRedirect ? JournalRedirectAction::kCloseJournal : JournalRedirectAction::kKeepJournal;
	}
}
