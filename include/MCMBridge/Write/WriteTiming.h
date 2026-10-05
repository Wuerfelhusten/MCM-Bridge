#pragma once

#include "MCMBridge/Core/OperationContext.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace MCMBridge
{
	enum class WriteTimingOutcome
	{
		kApplied,
		kRejected,
		kTimedOut,
		kStaleSnapshot,
		kCancelled,
		kPageRebuilt
	};

	struct WriteTimingReport
	{
		WriteTimingOutcome outcome{};
		std::string        reason;
		std::string        label;
		double             totalMilliseconds{};
		double             queueMilliseconds{};
		double             applyMilliseconds{};
		double             commitMilliseconds{};
		bool               started{};
		bool               hosted{};
		bool               commitStarted{};
	};

	// Construct before queuing; subsequent calls belong to the serialized game-task workflow.
	class WriteTiming
	{
	public:
		using Reporter = std::function<void(const WriteTimingReport&)>;

		explicit WriteTiming(
			Reporter               a_reporter,
			const IOperationClock& a_clock = SteadyOperationClock::GetSingleton());

		void BeginApply(bool a_hosted, std::string a_label);
		void BeginCommit();
		void Finish(WriteTimingOutcome a_outcome, std::string_view a_reason = {});

	private:
		Reporter                                  reporter;
		const IOperationClock&                    clock;
		IOperationClock::TimePoint                submittedAt;
		std::optional<IOperationClock::TimePoint> applyStartedAt;
		std::optional<IOperationClock::TimePoint> commitStartedAt;
		std::string                               label;
		bool                                      hosted{};
		bool                                      finished{};
	};
}
