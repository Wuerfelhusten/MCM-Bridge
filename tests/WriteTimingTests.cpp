#include "MCMBridge/Write/WriteQueue.h"
#include "MCMBridge/Write/WriteTiming.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <memory>
#include <stdexcept>
#include <vector>

using namespace std::chrono_literals;

namespace
{
	class FakeClock final : public MCMBridge::IOperationClock
	{
	public:
		TimePoint Now() const override { return now; }
		void      Advance(std::chrono::steady_clock::duration a_duration) { now += a_duration; }
		TimePoint now{};
	};

	struct TimingFixture
	{
		FakeClock                                 clock;
		std::vector<MCMBridge::WriteTimingReport> reports;

		std::shared_ptr<MCMBridge::WriteTiming> Create()
		{
			return std::make_shared<MCMBridge::WriteTiming>(
				[this](const auto& a_report) { reports.push_back(a_report); }, clock);
		}
	};
}

TEST_CASE("Write timing separates queue, apply, and hosted commit durations")
{
	TimingFixture fixture;
	auto          timing = fixture.Create();
	fixture.clock.Advance(12500us);
	timing->BeginApply(true, "Toggle");
	fixture.clock.Advance(80ms);
	timing->BeginCommit();
	fixture.clock.Advance(210ms);
	timing->Finish(MCMBridge::WriteTimingOutcome::kApplied);

	REQUIRE(fixture.reports.size() == 1);
	const auto& report = fixture.reports.front();
	CHECK(report.outcome == MCMBridge::WriteTimingOutcome::kApplied);
	CHECK(report.label == "Toggle");
	CHECK(report.reason.empty());
	CHECK(report.started);
	CHECK(report.hosted);
	CHECK(report.commitStarted);
	CHECK(report.totalMilliseconds == Catch::Approx(302.5));
	CHECK(report.queueMilliseconds == Catch::Approx(12.5));
	CHECK(report.applyMilliseconds == Catch::Approx(80.0));
	CHECK(report.commitMilliseconds == Catch::Approx(210.0));
	CHECK(report.totalMilliseconds == Catch::Approx(
										  report.queueMilliseconds + report.applyMilliseconds + report.commitMilliseconds));
}

TEST_CASE("Standalone timing includes its complete operation without a hosted commit phase")
{
	TimingFixture fixture;
	auto          timing = fixture.Create();
	timing->BeginCommit();
	fixture.clock.Advance(20ms);
	timing->BeginApply(false, "Slider");
	fixture.clock.Advance(140ms);
	timing->BeginCommit();
	fixture.clock.Advance(60ms);
	timing->Finish(MCMBridge::WriteTimingOutcome::kApplied);

	REQUIRE(fixture.reports.size() == 1);
	const auto& report = fixture.reports.front();
	CHECK(report.totalMilliseconds == Catch::Approx(220.0));
	CHECK(report.queueMilliseconds == Catch::Approx(20.0));
	CHECK(report.applyMilliseconds == Catch::Approx(200.0));
	CHECK(report.commitMilliseconds == 0.0);
	CHECK_FALSE(report.hosted);
	CHECK_FALSE(report.commitStarted);
}

TEST_CASE("Rejected and stale queued writes report time without an execution phase")
{
	TimingFixture fixture;
	auto          timing = fixture.Create();
	fixture.clock.Advance(70ms);
	auto outcome = MCMBridge::WriteTimingOutcome::kRejected;
	SECTION("Rejected") {}
	SECTION("Stale snapshot") { outcome = MCMBridge::WriteTimingOutcome::kStaleSnapshot; }
	timing->Finish(outcome, "Live value changed");

	REQUIRE(fixture.reports.size() == 1);
	const auto& report = fixture.reports.front();
	CHECK(report.outcome == outcome);
	CHECK(report.reason == "Live value changed");
	CHECK(report.totalMilliseconds == Catch::Approx(70.0));
	CHECK(report.queueMilliseconds == Catch::Approx(70.0));
	CHECK(report.applyMilliseconds == 0.0);
	CHECK(report.commitMilliseconds == 0.0);
	CHECK_FALSE(report.started);
}

TEST_CASE("Write timing includes a timeout in the phase that was active")
{
	TimingFixture fixture;
	auto          timing = fixture.Create();
	timing->BeginApply(true, "Menu");
	fixture.clock.Advance(30ms);
	bool commit{};
	SECTION("Apply callback timeout") {}
	SECTION("Hosted commit timeout")
	{
		commit = true;
		timing->BeginCommit();
	}
	fixture.clock.Advance(10s);
	timing->Finish(MCMBridge::WriteTimingOutcome::kTimedOut, "Papyrus call timed out");

	REQUIRE(fixture.reports.size() == 1);
	const auto& report = fixture.reports.front();
	CHECK(report.outcome == MCMBridge::WriteTimingOutcome::kTimedOut);
	CHECK(report.totalMilliseconds == Catch::Approx(10030.0));
	CHECK(report.applyMilliseconds == Catch::Approx(commit ? 30.0 : 10030.0));
	CHECK(report.commitMilliseconds == Catch::Approx(commit ? 10000.0 : 0.0));
}

TEST_CASE("Timing emits one cancellation and ignores late completions or duplicate phase starts")
{
	TimingFixture fixture;
	auto          timing = fixture.Create();
	fixture.clock.Advance(10ms);
	timing->BeginApply(true, "Keymap");
	fixture.clock.Advance(20ms);
	timing->BeginApply(true, "Replacement");
	timing->BeginCommit();
	fixture.clock.Advance(30ms);
	timing->BeginCommit();
	timing->Finish(MCMBridge::WriteTimingOutcome::kCancelled, "Loaded game changed");
	fixture.clock.Advance(1s);
	timing->BeginApply(false, "Late");
	timing->BeginCommit();
	timing->Finish(MCMBridge::WriteTimingOutcome::kApplied);

	REQUIRE(fixture.reports.size() == 1);
	const auto& report = fixture.reports.front();
	CHECK(report.outcome == MCMBridge::WriteTimingOutcome::kCancelled);
	CHECK(report.label == "Keymap");
	CHECK(report.totalMilliseconds == Catch::Approx(60.0));
	CHECK(report.applyMilliseconds == Catch::Approx(20.0));
	CHECK(report.commitMilliseconds == Catch::Approx(30.0));
}

TEST_CASE("Write timing preserves the submission timestamp when a command is requeued")
{
	TimingFixture         fixture;
	MCMBridge::WriteQueue queue;
	queue.Push({ .settingID = "setting", .timing = fixture.Create() });
	fixture.clock.Advance(40ms);
	auto first = queue.TryPop();
	REQUIRE(first);
	queue.PushFront(std::move(*first));
	fixture.clock.Advance(60ms);
	auto next = queue.TryPop();
	REQUIRE(next);
	next->timing->BeginApply(false, "Toggle");
	fixture.clock.Advance(25ms);
	next->timing->Finish(MCMBridge::WriteTimingOutcome::kApplied);
	REQUIRE(fixture.reports.size() == 1);
	CHECK(fixture.reports.front().queueMilliseconds == Catch::Approx(100.0));
	CHECK(fixture.reports.front().totalMilliseconds == Catch::Approx(125.0));
}

TEST_CASE("Clearing the write queue reports each pending command once without holding the queue lock")
{
	TimingFixture         fixture;
	MCMBridge::WriteQueue queue;
	queue.Push({ .settingID = "first", .timing = fixture.Create() });
	fixture.clock.Advance(15ms);
	queue.Push({ .settingID = "second", .timing = std::make_shared<MCMBridge::WriteTiming>([&](const auto& a_report) {
					CHECK_FALSE(queue.TryPop());
					fixture.reports.push_back(a_report);
				},
											fixture.clock) });
	fixture.clock.Advance(25ms);
	queue.Clear();
	queue.Clear();
	REQUIRE(fixture.reports.size() == 2);
	CHECK(fixture.reports[0].totalMilliseconds == Catch::Approx(40.0));
	CHECK(fixture.reports[1].totalMilliseconds == Catch::Approx(25.0));
	for (const auto& report : fixture.reports) {
		CHECK(report.outcome == MCMBridge::WriteTimingOutcome::kCancelled);
		CHECK_FALSE(report.started);
	}
}

TEST_CASE("Timing reporter failures and reentrant completion do not affect writes")
{
	FakeClock                               clock;
	std::size_t                             reports{};
	std::shared_ptr<MCMBridge::WriteTiming> timing;
	timing = std::make_shared<MCMBridge::WriteTiming>([&](const auto&) {
		++reports;
		timing->Finish(MCMBridge::WriteTimingOutcome::kRejected);
		throw std::runtime_error("Diagnostic sink failed");
	},
		clock);
	CHECK_NOTHROW(timing->Finish(MCMBridge::WriteTimingOutcome::kApplied));
	CHECK(reports == 1);
}
