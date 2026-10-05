#include "MCMBridge/Write/WritePauseState.h"
#include "MCMBridge/Write/WriteQueue.h"

#include <catch2/catch_test_macros.hpp>

using namespace std::chrono_literals;

TEST_CASE("Setting pause is enabled by default but starts only with a write")
{
	MCMBridge::WritePauseState state;
	CHECK(state.Enabled());
	CHECK_FALSE(state.ShouldPause());
	const auto ticket = state.Reserve();
	CHECK(state.Pending() == 1);
	CHECK_FALSE(state.ShouldPause());
	REQUIRE(state.Begin(ticket));
	CHECK(state.ShouldPause());
	CHECK(state.Complete(ticket));
	CHECK_FALSE(state.ShouldPause());
}

TEST_CASE("The first completed write cannot unpause a second queued write")
{
	MCMBridge::WritePauseState state;
	const auto                 first = state.Reserve();
	REQUIRE(state.Begin(first));
	const auto second = state.Reserve();
	REQUIRE(state.Complete(first));
	CHECK(state.ShouldPause());
	CHECK(state.Pending() == 1);
	REQUIRE(state.Begin(second));
	REQUIRE(state.Complete(second));
	CHECK_FALSE(state.ShouldPause());
	CHECK(state.Pending() == 0);
}

TEST_CASE("A write arriving before the menu closes revokes the pending unpause")
{
	MCMBridge::WritePauseState state;
	const auto                 first = state.Reserve();
	REQUIRE(state.Begin(first));
	REQUIRE(state.Complete(first));
	CHECK_FALSE(state.ShouldPause());
	const auto second = state.Reserve();
	CHECK(state.ShouldPause());
	state.MenuClosed();
	CHECK(state.ShouldPause());
	REQUIRE(state.Complete(second));
	state.MenuClosed();
	const auto third = state.Reserve();
	CHECK_FALSE(state.ShouldPause());
	REQUIRE(state.Begin(third));
	CHECK(state.ShouldPause());
}

TEST_CASE("Timeouts and duplicate completions release only their own pause tickets")
{
	MCMBridge::WritePauseState state;
	const auto                 timedOut = state.Reserve();
	const auto                 other = state.Reserve();
	REQUIRE(state.Begin(timedOut));
	REQUIRE(state.Complete(timedOut));
	CHECK_FALSE(state.Complete(timedOut));
	CHECK_FALSE(state.Complete(0));
	CHECK_FALSE(state.Begin(timedOut));
	CHECK(state.Pending() == 1);
	CHECK(state.ShouldPause());
	REQUIRE(state.Begin(other));
	REQUIRE(state.Complete(other));
	CHECK_FALSE(state.ShouldPause());
}

TEST_CASE("Turning the pause off and on preserves outstanding writes")
{
	MCMBridge::WritePauseState state;
	const auto                 first = state.Reserve();
	REQUIRE(state.Begin(first));
	state.SetEnabled(false);
	CHECK_FALSE(state.ShouldPause());
	const auto second = state.Reserve();
	REQUIRE(state.Begin(second));
	CHECK_FALSE(state.ShouldPause());
	state.MenuClosed();
	state.SetEnabled(true);
	CHECK(state.ShouldPause());
	REQUIRE(state.Complete(first));
	CHECK(state.ShouldPause());
	REQUIRE(state.Complete(second));
	CHECK_FALSE(state.ShouldPause());
}

TEST_CASE("Save invalidation expires tickets without reusing their IDs")
{
	MCMBridge::WritePauseState state;
	const auto                 old = state.Reserve();
	REQUIRE(state.Begin(old));
	state.Reset();
	CHECK(state.Pending() == 0);
	CHECK_FALSE(state.ShouldPause());
	const auto current = state.Reserve();
	CHECK(current != old);
	REQUIRE(state.Begin(current));
	CHECK_FALSE(state.Complete(old));
	CHECK_FALSE(state.Begin(old));
	CHECK(state.ShouldPause());
	REQUIRE(state.Complete(current));
	CHECK_FALSE(state.ShouldPause());
}

TEST_CASE("No callback may start until the engine confirms the pause")
{
	MCMBridge::WritePauseState                  state;
	const auto                                  ticket = state.Reserve();
	const std::chrono::steady_clock::time_point start{};
	const auto                                  waiting = state.AwaitPause(ticket, false, start);
	REQUIRE(waiting);
	CHECK_FALSE(*waiting);
	CHECK(state.ShouldPause());
	const auto ready = state.AwaitPause(ticket, true, start + 20ms);
	REQUIRE(ready);
	CHECK(*ready);
}

TEST_CASE("A missing engine pause times out using real time")
{
	MCMBridge::WritePauseState                  state;
	const auto                                  ticket = state.Reserve();
	const std::chrono::steady_clock::time_point start{};
	REQUIRE(state.AwaitPause(ticket, false, start));
	const auto waiting = state.AwaitPause(ticket, false, start + 1999ms);
	REQUIRE(waiting);
	CHECK_FALSE(*waiting);
	const auto expired = state.AwaitPause(ticket, false, start + 2s);
	REQUIRE_FALSE(expired);
	CHECK(expired.error().code == MCMBridge::BridgeErrorCode::kTimedOut);
	REQUIRE(state.Complete(ticket));
	CHECK_FALSE(state.ShouldPause());
	CHECK_FALSE(state.AwaitPause(ticket, true, start + 3s));
}

TEST_CASE("Journal pause is shared while edits continue and outlives Bridge completion")
{
	MCMBridge::WritePauseState state;
	bool                       journalOpen = true;
	bool                       bridgeOpen{};
	const auto                 paused = [&] { return journalOpen || bridgeOpen; };
	const auto                 ticket = state.Reserve();
	const auto                 waiting = state.AwaitPause(ticket, bridgeOpen, {});
	REQUIRE(waiting);
	CHECK_FALSE(*waiting);
	bridgeOpen = state.ShouldPause();
	CHECK(paused());
	const auto ready = state.AwaitPause(ticket, bridgeOpen, {});
	REQUIRE(ready);
	CHECK(*ready);
	CHECK(journalOpen);
	REQUIRE(state.Complete(ticket));
	bridgeOpen = state.ShouldPause();
	CHECK_FALSE(bridgeOpen);
	CHECK(paused());
	journalOpen = false;
	CHECK_FALSE(paused());
}

TEST_CASE("Closing the Journal cannot unpause a shared batch with pending writes")
{
	MCMBridge::WritePauseState state;
	bool                       journalOpen = true;
	bool                       bridgeOpen{};
	const auto                 paused = [&] { return journalOpen || bridgeOpen; };
	const auto                 first = state.Reserve();
	REQUIRE(state.Begin(first));
	const auto second = state.Reserve();
	bridgeOpen = state.ShouldPause();
	CHECK(paused());
	journalOpen = false;
	CHECK(paused());
	REQUIRE(state.Complete(first));
	bridgeOpen = state.ShouldPause();
	CHECK(paused());
	const auto ready = state.AwaitPause(second, bridgeOpen, {});
	REQUIRE(ready);
	CHECK(*ready);
	REQUIRE(state.Complete(second));
	bridgeOpen = state.ShouldPause();
	CHECK_FALSE(paused());
}

TEST_CASE("Disabling pause bypasses the engine gate without losing completion tracking")
{
	MCMBridge::WritePauseState state;
	state.SetEnabled(false);
	const auto ticket = state.Reserve();
	const auto ready = state.AwaitPause(ticket, false, {});
	REQUIRE(ready);
	CHECK(*ready);
	CHECK(state.Pending() == 1);
	CHECK_FALSE(state.ShouldPause());
	REQUIRE(state.Complete(ticket));
}

TEST_CASE("Expiring a queued write leaves active and unrelated writes alone")
{
	MCMBridge::WritePauseState state;
	MCMBridge::WriteQueue      queue;
	const auto                 active = state.Reserve();
	const auto                 expired = state.Reserve();
	const auto                 other = state.Reserve();
	REQUIRE(state.Begin(active));
	queue.Push({ .settingID = "expired", .pauseTicket = expired });
	queue.Push({ .settingID = "other", .pauseTicket = other });
	CHECK_FALSE(queue.Remove(active));
	auto removed = queue.Remove(expired);
	REQUIRE(removed);
	CHECK(removed->settingID == "expired");
	REQUIRE(state.Complete(removed->pauseTicket));
	CHECK_FALSE(queue.Remove(expired));
	CHECK(state.ShouldPause());
	CHECK(state.Pending() == 2);
	const auto next = queue.TryPop();
	REQUIRE(next);
	CHECK(next->pauseTicket == other);
	CHECK_FALSE(queue.TryPop());
}

TEST_CASE("A failed page removes only its queued writes in original order")
{
	MCMBridge::WriteQueue queue;
	queue.Push({ .settingID = "first", .expectedIdentity = { .pageKey = "Broken" }, .pauseTicket = 1 });
	queue.Push({ .settingID = "second", .expectedIdentity = { .pageKey = "Working" }, .pauseTicket = 2 });
	queue.Push({ .settingID = "third", .expectedIdentity = { .pageKey = "Broken" }, .pauseTicket = 3 });
	const auto removed = queue.RemoveIf([](const auto& a_command) { return a_command.expectedIdentity.pageKey == "Broken"; });
	REQUIRE(removed.size() == 2);
	CHECK(removed[0].pauseTicket == 1);
	CHECK(removed[1].pauseTicket == 3);
	const auto next = queue.TryPop();
	REQUIRE(next);
	CHECK(next->pauseTicket == 2);
}
