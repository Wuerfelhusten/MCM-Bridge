#include "MCMBridge/Core/OperationContext.h"
#include <algorithm>
#include <utility>

namespace MCMBridge
{
	const SteadyOperationClock& SteadyOperationClock::GetSingleton()
	{
		static SteadyOperationClock singleton;
		return singleton;
	}

	IOperationClock::TimePoint SteadyOperationClock::Now() const
	{
		return std::chrono::steady_clock::now();
	}

	OperationContext::OperationContext(
		std::chrono::steady_clock::duration                  a_budget,
		const IOperationClock&                               a_clock,
		std::function<std::chrono::steady_clock::duration()> a_wait) :
		clock(a_clock),
		budget(a_budget), wait(std::move(a_wait))
	{}

	void OperationContext::Start()
	{
		startedAt = clock.Now();
		startedWait = wait ? wait() : std::chrono::steady_clock::duration{};
		++token;
		active = true;
	}

	std::uint64_t OperationContext::BeginStep()
	{
		return ++token;
	}

	bool OperationContext::IsCurrent(std::uint64_t a_token) const
	{
		return active && a_token == token;
	}

	bool OperationContext::IsExpired() const
	{
		const auto excluded = wait ? std::max(wait() - startedWait, std::chrono::steady_clock::duration{}) : std::chrono::steady_clock::duration{};
		return !active || clock.Now() - startedAt - excluded > budget;
	}

	void OperationContext::Invalidate()
	{
		active = false;
		++token;
	}
}
