#pragma once

#include "MCMBridge/Core/OperationTimer.h"

#include <atomic>
#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace MCMBridge
{
	// Dispatch only enqueues work. The worker never executes a game operation.
	class DeadlineScheduler final : public IOperationTimer
	{
	public:
		using Dispatch = std::function<void(std::function<void()>)>;
		explicit DeadlineScheduler(Dispatch a_dispatch);
		~DeadlineScheduler();
		void          After(std::chrono::milliseconds a_delay, std::function<void()> a_task) override;
		std::uint64_t Schedule(std::chrono::milliseconds a_delay, std::function<void()> a_task) override;
		void          Cancel(std::uint64_t a_handle) override;
		void          Reset();
		std::size_t   Pending() const;
		std::uint64_t Scheduled() const;

	private:
		struct State
		{
			std::atomic_bool      cancelled{};
			std::function<void()> task;
		};
		using Key = std::pair<std::chrono::steady_clock::time_point, std::uint64_t>;
		void                                                                      Run(std::stop_token a_stop);
		Dispatch                                                                  dispatch;
		mutable std::mutex                                                        mutex;
		std::shared_ptr<std::condition_variable>                                  condition = std::make_shared<std::condition_variable>();
		std::map<Key, std::shared_ptr<State>>                                     deadlines;
		std::unordered_map<std::uint64_t, std::pair<Key, std::shared_ptr<State>>> handles;
		std::uint64_t                                                             nextHandle{ 1 };
		std::jthread                                                              worker;
	};
}
