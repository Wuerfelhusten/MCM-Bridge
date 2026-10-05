#pragma once

#include "MCMBridge/Core/DeadlineScheduler.h"
#include "MCMBridge/Core/OperationTimer.h"

#include <chrono>

namespace MCMBridge
{
	class TaskScheduler final : public IOperationTimer
	{
	public:
		static TaskScheduler& GetSingleton();
		void                  After(std::chrono::milliseconds a_delay, std::function<void()> a_task) override;
		std::uint64_t         Schedule(std::chrono::milliseconds a_delay, std::function<void()> a_task) override;
		void                  Cancel(std::uint64_t a_handle) override;
		void                  Reset();
		std::size_t           Pending() const { return scheduler.Pending(); }
		std::uint64_t         Scheduled() const { return scheduler.Scheduled(); }

	private:
		TaskScheduler();
		DeadlineScheduler scheduler;
	};
}
