#pragma once

#include "MCMBridge/Core/Result.h"
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace MCMBridge
{
	struct UnlockedRegistryEntry
	{
		std::string   id;
		std::string   displayName;
		std::uint64_t marker{};
	};

	// Callbacks and Poll run on the game thread. A result requires two matching reads.
	class UnlockedRegistryQuery : public std::enable_shared_from_this<UnlockedRegistryQuery>
	{
	public:
		using Clock = std::chrono::steady_clock;
		using Entries = std::vector<UnlockedRegistryEntry>;
		using CountReader = std::function<void(std::function<void(Result<std::int32_t>)>)>;
		using EntryReader = std::function<void(std::int32_t, std::function<void(Result<UnlockedRegistryEntry>)>)>;
		UnlockedRegistryQuery(CountReader a_count, EntryReader a_entry, std::function<Clock::time_point()> a_now);
		void            Start();
		Result<Entries> Poll();
		void            Cancel();
		void            SetCompletion(std::function<void()> a_completion) { completion = std::move(a_completion); }

	private:
		void                               BeginPass();
		void                               Pump();
		void                               CompletePass();
		bool                               Active();
		void                               Fail(BridgeError a_error);
		CountReader                        count;
		std::function<void()>              completion;
		EntryReader                        entry;
		std::function<Clock::time_point()> now;
		Clock::time_point                  deadline{};
		Entries                            first;
		Entries                            current;
		std::optional<Result<Entries>>     result;
		std::size_t                        next{};
		std::size_t                        pending{};
		bool                               verification{};
		bool                               started{};
		bool                               pumping{};
	};
}
