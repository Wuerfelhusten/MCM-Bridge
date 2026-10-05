#include "MCMBridge/Papyrus/HostedCloseOperation.h"

namespace
{
	constexpr auto callTimeout = std::chrono::seconds(10);
	constexpr auto operationBudget = std::chrono::seconds(15);
}

namespace MCMBridge
{
	HostedCloseOperation::HostedCloseOperation(
		std::shared_ptr<IClassicScript> a_script,
		Completion                      a_completion,
		IOperationTimer&                a_timer,
		const IOperationClock&          a_clock) :
		script(std::move(a_script)),
		completion(std::move(a_completion)),
		timer(a_timer),
		operation(operationBudget, a_clock, [this] { return script->MessageWaitDuration(); })
	{}

	void HostedCloseOperation::Start()
	{
		operation.Start();
		if (!script->IsConfigOpen()) {
			Finish({});
			return;
		}
		const auto token = operation.BeginStep();
		const auto dispatched = script->Dispatch(
			{ .method = ClassicMethod::kCloseConfig },
			[weak = weak_from_this(), token] {
				if (const auto self = weak.lock(); self && self->operation.IsCurrent(token)) {
					self->Finish({});
				}
			});
		if (!dispatched) {
			Finish(std::unexpected(BridgeError{ BridgeErrorCode::kDispatchFailed, "CloseConfig could not be dispatched" }));
			return;
		}
		if (!finished)
			ArmTimeout(token);
	}

	void HostedCloseOperation::Cancel()
	{
		if (!finished) {
			Finish(std::unexpected(BridgeError{ BridgeErrorCode::kStaleSnapshot, "Hosted close was cancelled" }));
		}
	}

	void HostedCloseOperation::Finish(Result<void> a_result)
	{
		if (finished) {
			return;
		}
		finished = true;
		deadline.Cancel();
		operation.Invalidate();
		if (!a_result && a_result.error().code == BridgeErrorCode::kTimedOut)
			script->RetireExecution();
		if (completion) {
			completion(std::move(a_result));
		}
	}

	void HostedCloseOperation::ArmTimeout(std::uint64_t a_token)
	{
		deadline.Arm(callTimeout, [weak = weak_from_this(), a_token] {
			if (const auto self = weak.lock(); self && !self->finished && self->operation.IsCurrent(a_token)) {
				self->Finish(std::unexpected(BridgeError{ BridgeErrorCode::kTimedOut, "CloseConfig timed out" }));
			} }, [target = script] { return target->MessageWaitDuration(); });
	}
}
