#pragma once

#include <chrono>
#include <cstdint>
#include <functional>

namespace MCMBridge
{
	class IOperationClock
	{
	public:
		using TimePoint = std::chrono::steady_clock::time_point;

		virtual ~IOperationClock() = default;
		virtual TimePoint Now() const = 0;
	};

	class SteadyOperationClock final : public IOperationClock
	{
	public:
		static const SteadyOperationClock& GetSingleton();
		TimePoint                          Now() const override;
	};

	class OperationContext
	{
	public:
		explicit OperationContext(
			std::chrono::steady_clock::duration                  a_budget,
			const IOperationClock&                               a_clock = SteadyOperationClock::GetSingleton(),
			std::function<std::chrono::steady_clock::duration()> a_wait = {});

		void          Start();
		std::uint64_t BeginStep();
		bool          IsCurrent(std::uint64_t a_token) const;
		bool          IsExpired() const;
		void          Invalidate();

	private:
		const IOperationClock&                               clock;
		std::chrono::steady_clock::duration                  budget;
		IOperationClock::TimePoint                           startedAt{};
		std::uint64_t                                        token{};
		bool                                                 active{};
		std::function<std::chrono::steady_clock::duration()> wait;
		std::chrono::steady_clock::duration                  startedWait{};
	};
}
