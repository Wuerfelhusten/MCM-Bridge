#include "MCMBridge/Papyrus/ClassicHelpOperation.h"

namespace
{
	constexpr auto callTimeout = std::chrono::seconds(10);
}

namespace MCMBridge
{
	ClassicHelpOperation::ClassicHelpOperation(
		std::shared_ptr<IClassicScript> a_script,
		SettingIdentity                 a_identity,
		BusyCheck                       a_busyCheck,
		Completion                      a_completion,
		IOperationTimer&                a_timer,
		const IOperationClock&          a_clock) :
		script(std::move(a_script)),
		identity(std::move(a_identity)),
		busyCheck(std::move(a_busyCheck)),
		completion(std::move(a_completion)),
		timer(a_timer),
		operation(callTimeout, a_clock, [this] { return script->MessageWaitDuration(); })
	{}

	void ClassicHelpOperation::Start()
	{
		operation.Start();
		if ((busyCheck && busyCheck()) || !script->IsConfigOpen() ||
			!script->IsPageReady(identity.pageIndex)) {
			Finish(std::unexpected(BridgeError{ BridgeErrorCode::kBusy, "Hosted MCM page is unavailable for help" }));
			return;
		}
		script->BeginInfoCapture();
		const auto token = operation.BeginStep();
		if (!script->Dispatch(
				{ .method = ClassicMethod::kHighlightOption, .integer = identity.optionIndex },
				[weak = weak_from_this(), token] {
					if (const auto self = weak.lock(); self && !self->finished && self->operation.IsCurrent(token)) {
						self->Finish(self->script->ReadInfoText());
					}
				})) {
			Finish(std::unexpected(BridgeError{ BridgeErrorCode::kDispatchFailed, "HighlightOption could not be dispatched" }));
			return;
		}
		if (!finished && operation.IsCurrent(token)) {
			ArmTimeout(token);
		}
	}

	void ClassicHelpOperation::Cancel()
	{
		Finish(std::unexpected(BridgeError{ BridgeErrorCode::kStaleSnapshot, "Help request was cancelled" }));
	}

	void ClassicHelpOperation::Finish(Result<std::string> a_result)
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

	void ClassicHelpOperation::ArmTimeout(std::uint64_t a_token)
	{
		deadline.Arm(callTimeout, [weak = weak_from_this(), a_token] {
			if (const auto self = weak.lock(); self && !self->finished && self->operation.IsCurrent(a_token)) {
				self->Finish(std::unexpected(BridgeError{ BridgeErrorCode::kTimedOut, "HighlightOption timed out" }));
			} }, [target = script] { return target->MessageWaitDuration(); });
	}
}
