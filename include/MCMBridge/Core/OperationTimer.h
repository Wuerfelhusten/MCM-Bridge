#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>

namespace MCMBridge
{
	class IOperationTimer
	{
	public:
		virtual ~IOperationTimer() = default;
		virtual void          After(std::chrono::milliseconds a_delay, std::function<void()> a_task) = 0;
		virtual std::uint64_t Schedule(std::chrono::milliseconds a_delay, std::function<void()> a_task)
		{
			After(a_delay, std::move(a_task));
			return 0;
		}
		virtual void Cancel(std::uint64_t) {}
	};

	class OperationDeadline
	{
	public:
		explicit OperationDeadline(IOperationTimer& a_timer) : timer(a_timer) {}
		~OperationDeadline() { Cancel(); }
		OperationDeadline(const OperationDeadline&) = delete;
		OperationDeadline& operator=(const OperationDeadline&) = delete;
		using WaitDuration = std::function<std::chrono::steady_clock::duration()>;
		// The provider reports cumulative user-dialog wait, never ordinary VM latency.
		// Arm, cancellation and timer callbacks all run on the operation thread.
		void Arm(std::chrono::milliseconds a_delay, std::function<void()> a_task, WaitDuration a_wait = {});
		void Cancel();

	private:
		struct State;
		static void            Schedule(const std::shared_ptr<State>& a_state, std::chrono::milliseconds a_delay);
		IOperationTimer&       timer;
		std::shared_ptr<State> state;
	};
}
