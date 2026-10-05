#include "MCMBridge/Papyrus/HostCallSession.h"

#include <limits>
#include <utility>

namespace MCMBridge
{
	HostCallSession::HostCallSession(std::shared_ptr<IClassicScript> a_script, IOperationTimer& a_timer) :
		script(std::move(a_script)), deadline(a_timer) {}
	HostCallSession::~HostCallSession() { Invalidate(); }

	bool HostCallSession::Submit(ClassicCall a_call, Completion a_completion, std::chrono::milliseconds a_timeout)
	{
		const auto owner = weak_from_this().lock();
		if (!owner || stopped || Busy() || !script || !a_completion || a_timeout.count() <= 0 || serial == std::numeric_limits<std::uint64_t>::max())
			return false;
		const auto current = ++serial;
		call = std::move(a_call);
		completion = std::move(a_completion);
		reopened = false;
		const auto target = script;
		if (call.method == ClassicMethod::kSelectOption) {
			const auto row = target->ReadSelectionControl(call.integer);
			if (row && (row->disabled || row->hidden || (row->type != MCMControlType::kText && row->type != MCMControlType::kToggle))) {
				Finish(HostCallStatus::kControlUnavailable);
				return true;
			}
			transitionControl = row && row->type == MCMControlType::kText ? row : std::nullopt;
			transitionPage = transitionControl ? target->ReadCurrentPage() : std::nullopt;
			transitionIndex = call.integer;
		}
		if (!target->Dispatch(call, [weak = weak_from_this(), current] {
				if (const auto self = weak.lock())
					self->Complete(current);
			})) {
			if (Busy() && serial == current)
				Finish(HostCallStatus::kDispatchFailed);
			return true;
		}
		if (Busy() && serial == current)
			deadline.Arm(a_timeout, [weak = weak_from_this(), current] {
				if (const auto self = weak.lock()) self->Expire(current); }, [target] { return target->MessageWaitDuration(); });
		return true;
	}

	bool HostCallSession::Current(std::uint64_t a_call)
	{
		if (!Busy() || serial != a_call || !script)
			return false;
		if (stopped) {
			Finish(HostCallStatus::kCancelled);
			return false;
		}
		return true;
	}

	void HostCallSession::Complete(std::uint64_t a_call)
	{
		if (!Current(a_call))
			return;
		if (call.method == ClassicMethod::kCloseConfig) {
			transitionControl.reset();
			transitionPage.reset();
		}
		if (call.method == ClassicMethod::kSetPage) {
			const auto page = script->ReadCurrentPage();
			if (!page || page->name != call.text || page->index != call.integer || !script->IsPageReady(call.integer)) {
				Finish(HostCallStatus::kInvalidData);
				return;
			}
			if (transitionControl && transitionPage && *page == *transitionPage) {
				const auto row = script->ReadSelectionControl(transitionIndex);
				const bool same = row && row->type == transitionControl->type && row->rawLabel == transitionControl->rawLabel &&
				                  row->identity.stateName == transitionControl->identity.stateName && !row->hidden;
				const bool reopen = same && row->disabled && row->value != transitionControl->value && !reopened;
				transitionControl = same && row->disabled ? row : std::nullopt;
				if (reopen) {
					// Some MCMs retain a session-local lock after initialization. Reopen
					// once per changed disabled value, never poll or repeat the click.
					reopened = true;
					ReopenPage(a_call);
					return;
				}
			} else {
				transitionControl.reset();
				transitionPage.reset();
			}
		}
		// Memory owns target comparison, refresh intervals and activation timeout.
		Finish(HostCallStatus::kCompleted);
	}

	void HostCallSession::ReopenPage(std::uint64_t a_call)
	{
		if (!script->Dispatch({ ClassicMethod::kCloseConfig }, [weak = weak_from_this(), a_call] {
				const auto self = weak.lock();
				if (!self || !self->Current(a_call))
					return;
				if (!self->script->Dispatch({ ClassicMethod::kOpenConfig }, [weak, a_call] {
						const auto current = weak.lock();
						if (!current || !current->Current(a_call))
							return;
						const auto pages = current->script->ReadNavigationPages();
						if (!pages || (current->call.integer >= 0 && (static_cast<std::size_t>(current->call.integer) >= pages->size() ||
																		 (*pages)[current->call.integer] != current->call.text))) {
							current->Finish(HostCallStatus::kControlUnavailable);
							return;
						}
						if (!current->script->Dispatch(current->call, [weak, a_call] {
								if (const auto next = weak.lock())
									next->Complete(a_call);
							}))
							current->Retire(HostCallStatus::kDispatchFailed);
					}))
					self->Retire(HostCallStatus::kDispatchFailed);
			}))
			Retire(HostCallStatus::kDispatchFailed);
	}

	void HostCallSession::Expire(std::uint64_t a_call)
	{
		if (Busy() && serial == a_call)
			Retire(HostCallStatus::kTimedOut);
	}
	void HostCallSession::Finish(HostCallStatus a_status)
	{
		deadline.Cancel();
		auto next = std::exchange(completion, {});
		if (next)
			next(a_status);
	}
	void HostCallSession::Cancel() { stopped = true; }
	bool HostCallSession::Close(Completion a_completion, std::chrono::milliseconds a_timeout)
	{
		if (Busy() || !script || !a_completion)
			return false;
		stopped = false;
		const bool accepted = Submit({ ClassicMethod::kCloseConfig }, [this, next = std::move(a_completion)](HostCallStatus a_status) {
			stopped = true;
			next(a_status); }, a_timeout);
		if (!accepted)
			stopped = true;
		return accepted;
	}
	void HostCallSession::Invalidate() { Retire(HostCallStatus::kSessionInvalidated); }
	void HostCallSession::Retire(HostCallStatus a_status)
	{
		stopped = true;
		deadline.Cancel();
		transitionControl.reset();
		transitionPage.reset();
		auto next = std::exchange(completion, {});
		if (auto target = std::exchange(script, {}))
			target->RetireExecution();
		if (next)
			next(a_status);
	}
}
