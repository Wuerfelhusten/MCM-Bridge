#include "MCMBridge/Plugin/BridgeController.h"

#include "MCMBridge/Plugin/TaskScheduler.h"
#include "MCMBridge/Plugin/WritePauseService.h"
#include "MCMBridge/Plugin/WriteTimingLog.h"
#include "MCMBridge/Write/WriteTiming.h"

namespace MCMBridge
{
	void BridgeController::Submit(WriteCommand a_command)
	{
		a_command.timing = MakeWriteTiming(a_command);
		if (auto* tasks = SKSE::GetTaskInterface()) {
			a_command.pauseTicket = WritePauseService::GetSingleton().Reserve();
			const auto ticket = a_command.pauseTicket;
			writes.Push(std::move(a_command));
			tasks->AddTask([] { GetSingleton().ProcessWrites(); });
			TaskScheduler::GetSingleton().After(std::chrono::seconds(60), [ticket] {
				auto& controller = GetSingleton();
				if (auto expired = controller.writes.Remove(ticket)) {
					controller.RejectWrite(*expired, { BridgeErrorCode::kTimedOut, "Setting change waited too long in the queue; no setting callback was started" });
					controller.ProcessWrites();
				}
			});
		} else {
			a_command.timing->Finish(WriteTimingOutcome::kRejected, "SKSE task interface is unavailable");
		}
	}

	void BridgeController::DeferWrite(WriteCommand a_command, std::chrono::milliseconds a_delay)
	{
		writes.PushFront(std::move(a_command));
		if (writeRetryScheduled)
			return;
		writeRetryScheduled = true;
		const auto operationSession = session;
		TaskScheduler::GetSingleton().After(a_delay, [operationSession] {
			auto& controller = GetSingleton();
			if (controller.session != operationSession)
				return;
			controller.writeRetryScheduled = false;
			controller.ProcessWrites();
		});
	}
}
