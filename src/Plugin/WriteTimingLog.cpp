#include "MCMBridge/Plugin/WriteTimingLog.h"

#include "MCMBridge/Write/WriteTiming.h"

#include <atomic>

namespace
{
	std::atomic<std::uint64_t> nextTimingID{ 1 };

	std::string_view OutcomeName(MCMBridge::WriteTimingOutcome a_outcome)
	{
		using Outcome = MCMBridge::WriteTimingOutcome;
		switch (a_outcome) {
		case Outcome::kApplied:
			return "applied";
		case Outcome::kRejected:
			return "rejected";
		case Outcome::kTimedOut:
			return "timed_out";
		case Outcome::kStaleSnapshot:
			return "stale_snapshot";
		case Outcome::kCancelled:
			return "cancelled";
		case Outcome::kPageRebuilt:
			return "page_rebuilt";
		}
		return "unknown";
	}

	std::string_view IntentName(MCMBridge::WriteIntent a_intent)
	{
		switch (a_intent) {
		case MCMBridge::WriteIntent::kSetValue:
			return "set";
		case MCMBridge::WriteIntent::kActivate:
			return "activate";
		case MCMBridge::WriteIntent::kReset:
			return "reset";
		}
		return "unknown";
	}
}

namespace MCMBridge
{
	std::shared_ptr<WriteTiming> MakeWriteTiming(const WriteCommand& a_command)
	{
		return std::make_shared<WriteTiming>([id = nextTimingID.fetch_add(1, std::memory_order_relaxed),
												 settingID = a_command.settingID,
												 identity = a_command.expectedIdentity,
												 intent = a_command.intent](const WriteTimingReport& a_report) {
			SKSE::log::info(
				"Setting timing id={} backend={} mode={} intent={} status={} "
				"total_ms={:.3f} queue_ms={:.3f} apply_ms={:.3f} commit_ms={:.3f} "
				"script={:?} page={:?} option={} setting={:?} label={:?} reason={:?}",
				id,
				identity.backend == MCMBackendKind::kMCMHelper ? "MCMHelper" : "ClassicSkyUI",
				a_report.started ? (a_report.hosted ? "hosted" : "standalone") : "not_started",
				IntentName(intent), OutcomeName(a_report.outcome),
				a_report.totalMilliseconds, a_report.queueMilliseconds, a_report.applyMilliseconds, a_report.commitMilliseconds,
				identity.scriptName, identity.pageKey, identity.optionIndex, settingID, a_report.label, a_report.reason);
			if (auto logger = spdlog::default_logger())
				logger->flush();
		});
	}
}
