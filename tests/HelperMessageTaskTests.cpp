#include "MCMBridge/Core/HelperMessageTask.h"

#include <coroutine>
#include <exception>
#include <utility>

#include "Co/Task.h"
#include <catch2/catch_test_macros.hpp>

namespace
{
	class Consumer
	{
	public:
		struct promise_type
		{
			Consumer            get_return_object() { return Consumer(std::coroutine_handle<promise_type>::from_promise(*this)); }
			std::suspend_never  initial_suspend() { return {}; }
			std::suspend_always final_suspend() noexcept { return {}; }
			void                return_void() {}
			void                unhandled_exception() { std::terminate(); }
		};
		explicit Consumer(std::coroutine_handle<promise_type> a_handle) : handle(a_handle) {}
		~Consumer() { handle.destroy(); }
		Consumer(const Consumer&) = delete;
		Consumer& operator=(const Consumer&) = delete;
		bool      Done() const { return handle.done(); }

	private:
		std::coroutine_handle<promise_type> handle;
	};

	Consumer Receive(void* a_address, int& a_resumed, bool& a_result)
	{
		Co::Task<bool> task{ Co::Task<bool>::from_address(a_address) };
		a_result = co_await task;
		++a_resumed;
	}
}

TEST_CASE("Native Helper message tasks satisfy the actual upstream coroutine consumer")
{
	MCMBridge::HelperMessageTask task;
	const auto                   foreign = Co::Task<bool>::from_address(task.Address());
	const auto*                  base = static_cast<const std::byte*>(task.Address());
	CHECK(reinterpret_cast<const std::byte*>(&foreign.promise().exception) - base == 0x10);
	CHECK(reinterpret_cast<const std::byte*>(&foreign.promise().continuation) - base == 0x20);
	CHECK(reinterpret_cast<const std::byte*>(&foreign.promise().value) - base == 0x28);
	CHECK_FALSE(task.Complete(true));
	int  resumed{};
	bool result{};
	auto consumer = Receive(task.Address(), resumed, result);
	CHECK_FALSE(consumer.Done());
	CHECK(resumed == 0);
	REQUIRE(task.Complete(true));
	CHECK(consumer.Done());
	CHECK(resumed == 1);
	CHECK(result);
	CHECK(task.Complete(false));
	CHECK(resumed == 1);
	CHECK(result);
}

TEST_CASE("Native Helper message cancellation and synchronous rejection preserve coroutine lifetime")
{
	MCMBridge::HelperMessageTask task;
	int                          resumed{};
	bool                         result{ true };
	auto                         consumer = Receive(task.Address(), resumed, result);
	std::coroutine_handle<>::from_address(task.Address()).destroy();
	CHECK(task.Cancelled());
	CHECK(task.Complete(false));
	CHECK_FALSE(consumer.Done());
	CHECK(resumed == 0);
	for (int iteration = 0; iteration < 2233; ++iteration) {
		auto rejected = Receive(MCMBridge::HelperMessageTask::Rejected(), resumed, result);
		CHECK(rejected.Done());
		CHECK_FALSE(result);
		std::coroutine_handle<>::from_address(MCMBridge::HelperMessageTask::Rejected()).destroy();
	}
	CHECK(resumed == 2233);
}

TEST_CASE("Native Helper message tasks deliver repeated accept and reject answers once")
{
	int resumed{};
	for (int iteration = 0; iteration < 2233; ++iteration) {
		MCMBridge::HelperMessageTask task;
		const bool                   expected = iteration % 2 != 0;
		bool                         result = !expected;
		auto                         consumer = Receive(task.Address(), resumed, result);
		CHECK(task.Complete(expected));
		CHECK(consumer.Done());
		CHECK(result == expected);
		CHECK(resumed == iteration + 1);
	}
}

TEST_CASE("Abandoned Helper messages cannot resume a discarded VM continuation")
{
	MCMBridge::HelperMessageTask task;
	int                          resumed{};
	bool                         result{};
	auto                         consumer = Receive(task.Address(), resumed, result);
	task.Abandon();
	CHECK(task.Complete(true));
	CHECK(task.Complete(false));
	CHECK(resumed == 0);
	CHECK_FALSE(consumer.Done());
}
