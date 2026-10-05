#include "MCMBridge/Core/NativeHostSession.h"
#include "MCMBridge/Core/OperationTimer.h"

#include <catch2/catch_test_macros.hpp>
#include <map>

namespace
{
	using namespace MCMBridge;
	using namespace std::chrono_literals;

	class Clock final : public IOperationClock, public IOperationTimer
	{
	public:
		TimePoint     Now() const override { return now; }
		void          After(std::chrono::milliseconds a_delay, std::function<void()> a_task) override { Schedule(a_delay, std::move(a_task)); }
		std::uint64_t Schedule(std::chrono::milliseconds a_delay, std::function<void()> a_task) override
		{
			const auto id = ++serial;
			tasks.emplace(std::pair{ now + a_delay, id }, std::move(a_task));
			return id;
		}
		void Cancel(std::uint64_t a_id) override
		{
			std::erase_if(tasks, [a_id](const auto& a_entry) { return a_entry.first.second == a_id; });
		}
		void Advance(std::chrono::milliseconds a_duration)
		{
			const auto end = now + a_duration;
			while (!tasks.empty() && tasks.begin()->first.first <= end) {
				auto node = tasks.extract(tasks.begin());
				now = node.key().first;
				node.mapped()();
			}
			now = end;
		}
		std::size_t Pending() const { return tasks.size(); }

	private:
		TimePoint                                                            now{};
		std::uint64_t                                                        serial{};
		std::map<std::pair<TimePoint, std::uint64_t>, std::function<void()>> tasks;
	};
}

TEST_CASE("User message time is excluded from callback and operation budgets")
{
	Clock             clock;
	NativeHostSession host(clock);
	host.Reset(1);
	const auto token = host.Open(1, "mod", 123);
	REQUIRE(token);
	const auto        wait = [&] { return host.MessageWaitDuration(*token); };
	OperationContext  operation(60s, clock, wait);
	OperationDeadline deadline(clock);
	int               expired{};
	operation.Start();
	deadline.Arm(10s, [&] { ++expired; }, wait);
	clock.Advance(2s);
	const auto message = host.BeginMessage(*token);
	REQUIRE(message > 0);
	clock.Advance(120s);
	CHECK(expired == 0);
	CHECK_FALSE(operation.IsExpired());
	CHECK(clock.Pending() == 1);
	CHECK(wait() == 120s);
	REQUIRE(host.CompleteMessage(*token, message, true));
	CHECK(host.TakeMessage(*token, message) == 1);
	clock.Advance(7999ms);
	CHECK(expired == 0);
	clock.Advance(1ms);
	CHECK(expired == 1);
	CHECK(clock.Pending() == 0);
	clock.Advance(50001ms);
	CHECK(operation.IsExpired());
}

TEST_CASE("Completed messages still extend deadlines and successor calls get fresh budgets")
{
	Clock             clock;
	NativeHostSession host(clock);
	host.Reset(1);
	const auto token = host.Open(1, "mod");
	REQUIRE(token);
	const auto        wait = [&] { return host.MessageWaitDuration(*token); };
	OperationDeadline deadline(clock);
	int               expired{};
	deadline.Arm(10s, [&] { ++expired; }, wait);
	clock.Advance(1s);
	const auto first = host.BeginMessage(*token);
	clock.Advance(2s);
	REQUIRE(host.CompleteMessage(*token, first, false));
	CHECK(host.TakeMessage(*token, first) == 0);
	clock.Advance(1s);
	const auto second = host.BeginMessage(*token);
	clock.Advance(3s);
	REQUIRE(host.CompleteMessage(*token, second, true));
	CHECK(host.TakeMessage(*token, second) == 1);
	clock.Advance(7999ms);
	CHECK(expired == 0);
	clock.Advance(1ms);
	CHECK(expired == 1);
	deadline.Arm(10s, [&] { ++expired; }, wait);
	clock.Advance(9999ms);
	CHECK(expired == 1);
	clock.Advance(1ms);
	CHECK(expired == 2);
}

TEST_CASE("Message deadlines can be cancelled while waiting without late callbacks")
{
	Clock             clock;
	NativeHostSession host(clock);
	host.Reset(1);
	const auto token = host.Open(1, "mod");
	REQUIRE(token);
	OperationDeadline deadline(clock);
	int               expired{};
	deadline.Arm(10s, [&] { ++expired; }, [&] { return host.MessageWaitDuration(*token); });
	const auto message = host.BeginMessage(*token);
	clock.Advance(90s);
	deadline.Cancel();
	host.Reset(2);
	CHECK_FALSE(host.CompleteMessage(*token, message, true));
	clock.Advance(120s);
	CHECK(expired == 0);
	CHECK(clock.Pending() == 0);
}

TEST_CASE("Menu ownership and dialog waits do not leak across scripts or sessions")
{
	Clock             clock;
	NativeHostSession host(clock);
	host.Reset(1);
	const auto token = host.Open(1, "mod", 123);
	REQUIRE(token);
	CHECK(host.TokenForOwner(123) == *token);
	CHECK(host.TokenForOwner(456) == 0);
	const auto message = host.BeginMessage(*token);
	clock.Advance(30s);
	CHECK(host.MessageWaitDuration(*token) == 30s);
	CHECK(host.MessageWaitDuration(*token + 1) == 0s);
	REQUIRE(host.Close(*token));
	CHECK(host.TokenForOwner(123) == 0);
	const auto next = host.Open(1, "other", 456);
	REQUIRE(next);
	CHECK_FALSE(host.CompleteMessage(*token, message, true));
	CHECK(host.MessageWaitDuration(*next) == 0s);
	CHECK(host.TokenForOwner(123) == 0);
	CHECK(host.TokenForOwner(456) == *next);
}
