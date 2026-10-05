#include "MCMBridge/Core/DeadlineScheduler.h"

#include <catch2/catch_test_macros.hpp>
#include <condition_variable>
#include <vector>

namespace
{
	struct GameQueue
	{
		void Submit(std::function<void()> a_task)
		{
			const std::scoped_lock lock(mutex);
			tasks.push_back(std::move(a_task));
			condition.notify_all();
		}
		bool Wait(std::size_t a_count)
		{
			std::unique_lock lock(mutex);
			return condition.wait_for(lock, std::chrono::seconds(5), [&] { return tasks.size() >= a_count; });
		}
		void Run()
		{
			std::vector<std::function<void()>> pending;
			{
				const std::scoped_lock lock(mutex);
				pending.swap(tasks);
			}
			for (auto& task : pending) task();
		}
		std::mutex                         mutex;
		std::condition_variable            condition;
		std::vector<std::function<void()>> tasks;
	};
}

TEST_CASE("Deadlines dispatch in order without executing on the timer thread")
{
	GameQueue                    queue;
	MCMBridge::DeadlineScheduler timer([&](auto a_task) { queue.Submit(std::move(a_task)); });
	std::vector<int>             calls;
	timer.After(std::chrono::milliseconds(30), [&] { calls.push_back(2); });
	timer.After(std::chrono::milliseconds(0), [&] { calls.push_back(1); });
	REQUIRE(queue.Wait(2));
	REQUIRE(calls.empty());
	queue.Run();
	REQUIRE(calls == std::vector<int>{ 1, 2 });
	REQUIRE(timer.Pending() == 0);
}

TEST_CASE("Cancelled and reset deadlines cannot run after game queue submission")
{
	GameQueue                    queue;
	MCMBridge::DeadlineScheduler timer([&](auto a_task) { queue.Submit(std::move(a_task)); });
	int                          calls{};
	const auto                   handle = timer.Schedule(std::chrono::milliseconds(0), [&] { ++calls; });
	REQUIRE(queue.Wait(1));
	timer.Cancel(handle);
	queue.Run();
	REQUIRE(calls == 0);
	timer.After(std::chrono::milliseconds(0), [&] { ++calls; });
	REQUIRE(queue.Wait(1));
	timer.Reset();
	queue.Run();
	REQUIRE(calls == 0);
	REQUIRE(timer.Pending() == 0);
}

TEST_CASE("Serial deadline load does not accumulate timeouts")
{
	GameQueue                    queue;
	MCMBridge::DeadlineScheduler timer([&](auto a_task) { queue.Submit(std::move(a_task)); });
	int                          calls{};
	for (int index = 0; index < 2200; ++index) {
		MCMBridge::OperationDeadline timeout(timer);
		timeout.Arm(std::chrono::seconds(10), [] {});
		timer.After(std::chrono::milliseconds(0), [&] { ++calls; });
		REQUIRE(queue.Wait(1));
		queue.Run();
		timeout.Cancel();
		REQUIRE(timer.Pending() == 0);
	}
	REQUIRE(calls == 2200);
}

TEST_CASE("Scheduler destruction invalidates tasks already delivered to the game queue")
{
	GameQueue queue;
	int       calls{};
	{
		MCMBridge::DeadlineScheduler timer([&](auto a_task) { queue.Submit(std::move(a_task)); });
		timer.After(std::chrono::milliseconds(0), [&] { ++calls; });
		REQUIRE(queue.Wait(1));
	}
	queue.Run();
	CHECK(calls == 0);
}
