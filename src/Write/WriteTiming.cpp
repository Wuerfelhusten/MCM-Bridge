#include "MCMBridge/Write/WriteTiming.h"

#include <utility>

namespace
{
	double Milliseconds(MCMBridge::IOperationClock::TimePoint a_start, MCMBridge::IOperationClock::TimePoint a_end)
	{
		return std::chrono::duration<double, std::milli>(a_end - a_start).count();
	}
}

namespace MCMBridge
{
	WriteTiming::WriteTiming(Reporter a_reporter, const IOperationClock& a_clock) :
		reporter(std::move(a_reporter)),
		clock(a_clock),
		submittedAt(a_clock.Now())
	{}

	void WriteTiming::BeginApply(bool a_hosted, std::string a_label)
	{
		if (!finished && !applyStartedAt) {
			applyStartedAt = clock.Now();
			hosted = a_hosted;
			label = std::move(a_label);
		}
	}

	void WriteTiming::BeginCommit()
	{
		if (!finished && hosted && applyStartedAt && !commitStartedAt) {
			commitStartedAt = clock.Now();
		}
	}

	void WriteTiming::Finish(WriteTimingOutcome a_outcome, std::string_view a_reason)
	{
		if (finished) {
			return;
		}
		const auto completedAt = clock.Now();
		finished = true;
		WriteTimingReport report{
			.outcome = a_outcome,
			.reason = std::string(a_reason),
			.label = label,
			.totalMilliseconds = Milliseconds(submittedAt, completedAt),
			.queueMilliseconds = Milliseconds(submittedAt, applyStartedAt.value_or(completedAt)),
			.applyMilliseconds = applyStartedAt ?
			                         Milliseconds(*applyStartedAt, commitStartedAt.value_or(completedAt)) :
			                         0.0,
			.commitMilliseconds = commitStartedAt ? Milliseconds(*commitStartedAt, completedAt) : 0.0,
			.started = applyStartedAt.has_value(),
			.hosted = hosted,
			.commitStarted = commitStartedAt.has_value()
		};
		if (reporter) {
			try {
				reporter(report);
			} catch (...) {
				// A diagnostic sink must not change the outcome of an MCM operation.
			}
		}
	}
}
