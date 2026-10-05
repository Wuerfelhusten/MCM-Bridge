#include "MCMBridge/Plugin/TaskScheduler.h"

#include "SKSE/SKSE.h"

namespace MCMBridge
{
	TaskScheduler::TaskScheduler() : scheduler([](std::function<void()> a_task) {
			if (auto* tasks = SKSE::GetTaskInterface())
				tasks->AddTask(std::move(a_task));
			else
				throw std::runtime_error("Game task interface is unavailable");
		}) {}
	TaskScheduler& TaskScheduler::GetSingleton()
	{
		static TaskScheduler singleton;
		return singleton;
	}

	void TaskScheduler::After(std::chrono::milliseconds a_delay, std::function<void()> a_task)
	{
		scheduler.After(a_delay, std::move(a_task));
	}
	std::uint64_t TaskScheduler::Schedule(std::chrono::milliseconds a_delay, std::function<void()> a_task) { return scheduler.Schedule(a_delay, std::move(a_task)); }
	void          TaskScheduler::Cancel(std::uint64_t a_handle) { scheduler.Cancel(a_handle); }
	void          TaskScheduler::Reset()
	{
		scheduler.Reset();
	}
}
