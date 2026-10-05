#include "MCMBridge/Papyrus/ClassicWriteOperation.h"

#include <format>

namespace
{
	constexpr auto callTimeout = std::chrono::seconds(10);
}

namespace MCMBridge
{
	void ClassicWriteOperation::DispatchDialog(ClassicCall a_call, std::function<void()> a_next)
	{
		if (finished || !CheckBudget())
			return;
		if (busyCheck && busyCheck()) {
			YieldToFrontend();
			return;
		}
		if (!finished)
			Dispatch(std::move(a_call), std::move(a_next));
	}

	bool ClassicWriteOperation::CheckBudget()
	{
		if (!operation.IsExpired()) {
			return true;
		}
		Finish(std::unexpected(BridgeError{ BridgeErrorCode::kTimedOut, "MCM write exceeded its operation budget" }));
		return false;
	}

	bool ClassicWriteOperation::Dispatch(
		ClassicCall           a_call,
		std::function<void()> a_next)
	{
		if (finished || !CheckBudget()) {
			return false;
		}
		if (a_call.method != ClassicMethod::kCloseConfig && busyCheck && busyCheck()) {
			YieldToFrontend();
			return false;
		}
		const auto token = operation.BeginStep();
		const auto methodName = ClassicMethodName(a_call.method);
		if (!script->Dispatch(std::move(a_call), [weak = weak_from_this(), token, next = std::move(a_next)]() mutable {
				if (auto self = weak.lock()) {
					self->Continue(token, std::move(next));
				}
			})) {
			BridgeError error{ BridgeErrorCode::kDispatchFailed, std::format("Papyrus call {} could not be dispatched", methodName) };
			if (closing) {
				configOpen = false;
				closing = false;
				Finish(std::unexpected(std::move(error)));
			} else {
				Close(std::unexpected(std::move(error)));
			}
			return false;
		}
		if (!finished && operation.IsCurrent(token)) {
			ArmTimeout(token);
		}
		return true;
	}

	void ClassicWriteOperation::Continue(std::uint64_t a_token, std::function<void()> a_next)
	{
		if (finished || !operation.IsCurrent(a_token))
			return;
		deadline.Cancel();
		if (!closing && busyCheck && busyCheck()) {
			YieldToFrontend();
			return;
		}
		if ((closing || CheckBudget()) && a_next)
			a_next();
	}

	void ClassicWriteOperation::ArmTimeout(std::uint64_t a_token)
	{
		deadline.Arm(callTimeout, [weak = weak_from_this(), a_token] {
			if (auto self = weak.lock(); self && !self->finished && self->operation.IsCurrent(a_token)) {
				self->Finish(std::unexpected(BridgeError{ BridgeErrorCode::kTimedOut, "Papyrus write call timed out" }));
			} }, [target = script] { return target->MessageWaitDuration(); });
	}
}
