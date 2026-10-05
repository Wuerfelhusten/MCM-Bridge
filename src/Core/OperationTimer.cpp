#include "MCMBridge/Core/OperationTimer.h"

#include <utility>

namespace MCMBridge
{
	struct OperationDeadline::State
	{
		IOperationTimer&                    timer;
		std::function<void()>               task;
		WaitDuration                        wait;
		std::chrono::steady_clock::duration accounted{};
		std::uint64_t                       handle{};
		bool                                active{ true };
	};

	void OperationDeadline::Arm(std::chrono::milliseconds a_delay, std::function<void()> a_task, WaitDuration a_wait)
	{
		Cancel();
		state = std::make_shared<State>(State{ timer, std::move(a_task), std::move(a_wait) });
		if (state->wait)
			state->accounted = state->wait();
		Schedule(state, a_delay);
	}

	void OperationDeadline::Schedule(const std::shared_ptr<State>& a_state, std::chrono::milliseconds a_delay)
	{
		a_state->handle = a_state->timer.Schedule(a_delay, [weak = std::weak_ptr(a_state)] {
			const auto current = weak.lock();
			if (!current || !current->active)
				return;
			const auto waited = current->wait ? current->wait() : current->accounted;
			if (waited > current->accounted) {
				const auto delay = std::chrono::ceil<std::chrono::milliseconds>(waited - current->accounted);
				current->accounted = waited;
				Schedule(current, delay);
				return;
			}
			current->active = false;
			auto task = std::move(current->task);
			if (task)
				task();
		});
	}

	void OperationDeadline::Cancel()
	{
		if (auto previous = std::exchange(state, {})) {
			previous->active = false;
			previous->timer.Cancel(previous->handle);
		}
	}
}
