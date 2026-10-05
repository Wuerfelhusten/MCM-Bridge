#include "MCMBridge/Core/DeadlineScheduler.h"

#include <algorithm>

namespace MCMBridge
{
	DeadlineScheduler::DeadlineScheduler(Dispatch a_dispatch) : dispatch(std::move(a_dispatch)), worker([this](std::stop_token a_stop) { Run(a_stop); }) {}
	DeadlineScheduler::~DeadlineScheduler()
	{
		{
			const std::scoped_lock lock(mutex);
			worker.request_stop();
		}
		condition->notify_all();
		worker.join();
		Reset();
	}
	void          DeadlineScheduler::After(std::chrono::milliseconds a_delay, std::function<void()> a_task) { Schedule(a_delay, std::move(a_task)); }
	std::uint64_t DeadlineScheduler::Schedule(std::chrono::milliseconds a_delay, std::function<void()> a_task)
	{
		const std::scoped_lock lock(mutex);
		const auto             id = nextHandle++;
		std::erase_if(handles, [](const auto& a_entry) { return a_entry.second.second->cancelled.load(); });
		auto state = std::make_shared<State>();
		state->task = std::move(a_task);
		Key key{ std::chrono::steady_clock::now() + a_delay, id };
		deadlines.emplace(key, state);
		handles.emplace(id, std::pair{ key, state });
		condition->notify_one();
		return id;
	}
	void DeadlineScheduler::Cancel(std::uint64_t a_handle)
	{
		const std::scoped_lock lock(mutex);
		if (const auto found = handles.find(a_handle); found != handles.end()) {
			found->second.second->cancelled.store(true);
			deadlines.erase(found->second.first);
			handles.erase(found);
			condition->notify_one();
		}
	}
	void DeadlineScheduler::Reset()
	{
		const std::scoped_lock lock(mutex);
		for (const auto& [id, entry] : handles) {
			static_cast<void>(id);
			entry.second->cancelled.store(true);
		}
		handles.clear();
		deadlines.clear();
		condition->notify_one();
	}
	std::size_t DeadlineScheduler::Pending() const
	{
		const std::scoped_lock lock(mutex);
		return static_cast<std::size_t>(std::count_if(handles.begin(), handles.end(), [](const auto& a_entry) { return !a_entry.second.second->cancelled.load(); }));
	}
	std::uint64_t DeadlineScheduler::Scheduled() const
	{
		const std::scoped_lock lock(mutex);
		return nextHandle - 1;
	}
	void DeadlineScheduler::Run(std::stop_token a_stop)
	{
		std::unique_lock lock(mutex);
		while (!a_stop.stop_requested()) {
			std::erase_if(handles, [](const auto& a_entry) { return a_entry.second.second->cancelled.load(); });
			if (deadlines.empty()) {
				condition->wait(lock);
				continue;
			}
			const auto due = deadlines.begin()->first.first;
			if (due > std::chrono::steady_clock::now()) {
				condition->wait_until(lock, due);
				continue;
			}
			auto state = deadlines.begin()->second;
			deadlines.erase(deadlines.begin());
			lock.unlock();
			try {
				dispatch([state, wake = condition] {
					const auto claimed = !state->cancelled.exchange(true);
					wake->notify_one();
					if (claimed && state->task)
						state->task();
				});
			} catch (...) {
				state->cancelled.store(true);
			}
			lock.lock();
		}
	}
}
