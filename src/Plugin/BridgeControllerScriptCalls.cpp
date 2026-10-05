#include "MCMBridge/Plugin/BridgeController.h"

#include "MCMBridge/Papyrus/NativeFacade.h"
#include "MCMBridge/Papyrus/NativeHostUI.h"
#include "MCMBridge/Plugin/TaskScheduler.h"
#include "MCMBridge/Plugin/WritePauseService.h"
#include "MCMBridge/UI/WriteNotifications.h"

namespace MCMBridge
{
	void BridgeController::BeginScriptCall(std::uint64_t a_session, std::int32_t a_request,
		RE::BSTSmartPointer<RE::BSScript::Object> a_script, RE::BSTSmartPointer<RE::BSScript::Stack> a_stack, std::string a_method)
	{
		auto& requests = NativeCallRequests();
		if (!requests.Claim(a_session, a_request))
			return;
		const auto reject = [&] { requests.Complete(a_session, a_request, -1); };
		if (a_session != session || !IsSessionReady() || !a_script || !a_stack || scriptContext.closing) {
			reject();
			return;
		}
		const auto identity = registry.Native().ResolveIdentity(a_script);
		NativeHostUI::CollectScriptStacks();
		if (!identity || quarantined.contains(*identity)) {
			reject();
			return;
		}
		const auto kind = a_method == "OpenConfig" ? FacadeCallKind::kOpen : a_method == "CloseConfig" ? FacadeCallKind::kClose :
		                                                                 a_method == "SetPage"         ? FacadeCallKind::kPage :
		                                                                                                 FacadeCallKind::kOperation;
		if (!scriptContext.calls.Token() && kind != FacadeCallKind::kOpen &&
			!BorrowScriptContext(a_script, *identity, a_stack->stackID)) {
			reject();
			return;
		}
		if (!scriptContext.calls.Token()) {
			if (kind != FacadeCallKind::kOpen || directContext.id) {
				reject();
				return;
			}
			const auto    owner = std::format("Papyrus:{}", a_stack->stackID);
			std::uint64_t lease{};
			if (BeginExternalOperation(owner, lease) != MCM_HOST_OK) {
				CancelExternalOperation(owner);
				SKSE::log::warn("External Papyrus config rejected while host is busy: mod={}", *identity);
				reject();
				return;
			}
			const auto token = NativeFacadeSession().Open(session, *identity, reinterpret_cast<std::uintptr_t>(a_script.get()));
			if (!token || !registry.Native().SetActive(a_script)) {
				if (token)
					NativeFacadeSession().Close(*token);
				EndExternalOperation(lease);
				reject();
				return;
			}
			scriptContext.calls.Open(session, reinterpret_cast<std::uintptr_t>(a_script.get()), *token);
			scriptContext.script = a_script;
			scriptContext.owner = owner;
			scriptContext.mod = *identity;
			scriptContext.lease = lease;
		}
		const auto permit = scriptContext.calls.Enter(session, reinterpret_cast<std::uintptr_t>(a_script.get()), a_stack->stackID, kind);
		if (!permit) {
			reject();
			return;
		}
		scriptContext.admitted.emplace(a_request, ScriptContext::Call{ *permit, kind == FacadeCallKind::kClose });
		const bool first = !scriptContext.stack;
		scriptContext.stack = a_stack;
		NativeHostUI::AttachScriptStack(a_stack, scriptContext.calls.Token(), a_script.get());
		if (first) {
			TaskScheduler::GetSingleton().Cancel(scriptContext.timer);
			scriptContext.executionDeadline = std::make_unique<OperationDeadline>(TaskScheduler::GetSingleton());
			scriptContext.executionDeadline->Arm(std::chrono::seconds(30), [this, a_session, permit = *permit] {
				if (session == a_session && scriptContext.calls.Running(permit))
					RetireScriptContext(true); }, [token = scriptContext.calls.Token()] { return NativeFacadeSession().MessageWaitDuration(token); });
		}
		const bool mutation = a_method == "SelectOption" || a_method == "ResetOption" || a_method == "RemapKey" ||
		                      a_method == "SetSliderValue" || a_method == "SetMenuIndex" || a_method == "SetColorValue" || a_method == "SetInputText";
		if (mutation && !scriptContext.pause)
			scriptContext.pause = WritePauseService::GetSingleton().Reserve();
		ReadyScriptCall(a_session, a_request, *permit);
	}

	void BridgeController::ReadyScriptCall(std::uint64_t a_session, std::int32_t a_request, std::uint64_t a_permit)
	{
		if (session != a_session || !scriptContext.calls.Running(a_permit))
			return;
		if (scriptContext.pause) {
			const auto ready = WritePauseService::GetSingleton().Begin(scriptContext.pause);
			if (!ready) {
				RetireScriptContext();
				NativeCallRequests().Complete(a_session, a_request, -1);
				return;
			}
			if (!*ready) {
				TaskScheduler::GetSingleton().After(std::chrono::milliseconds(16), [this, a_session, a_request, a_permit] {
					ReadyScriptCall(a_session, a_request, a_permit);
				});
				return;
			}
		}
		NativeCallRequests().Complete(a_session, a_request, scriptContext.calls.Token());
	}

	void BridgeController::FinishScriptCall(std::uint64_t a_session, std::int32_t a_request, std::int32_t a_admission, std::uint32_t a_stack, bool a_valid)
	{
		auto& requests = NativeCallRequests();
		if (!requests.Claim(a_session, a_request))
			return;
		const auto entry = scriptContext.admitted.find(a_admission);
		if (a_session != session || entry == scriptContext.admitted.end() || !scriptContext.calls.Leave(entry->second.permit, a_stack)) {
			requests.Complete(a_session, a_request, -1);
			return;
		}
		const auto closing = entry->second.closing;
		scriptContext.admitted.erase(entry);
		if (!a_valid) {
			quarantined.insert(scriptContext.mod);
			SKSE::log::error("External Papyrus callback produced invalid page buffers: mod={}", scriptContext.mod);
			RetireScriptContext();
			requests.Complete(a_session, a_request, -1);
			return;
		}
		if (closing) {
			if (scriptContext.borrowed) {
				const std::scoped_lock lock(hostedRequestMutex);
				if (viewLoad.Current(scriptContext.viewRevision)) {
					hostedPageRoute.Close(requestedHostedModID, requestedHostedPageID);
					requestedHostedModID.clear();
					requestedHostedPageID.clear();
				}
			}
			RetireScriptContext();
		} else if (!scriptContext.calls.Busy() && scriptContext.borrowed) {
			ReturnScriptContext();
		} else if (!scriptContext.calls.Busy()) {
			WritePauseService::GetSingleton().Complete(std::exchange(scriptContext.pause, 0));
			NativeHostUI::DetachScriptStack(scriptContext.stack.get());
			scriptContext.stack.reset();
			TaskScheduler::GetSingleton().Cancel(scriptContext.timer);
			scriptContext.executionDeadline.reset();
			const auto token = scriptContext.calls.Token();
			// An abandoned external client cannot retain execution ownership forever.
			scriptContext.timer = TaskScheduler::GetSingleton().Schedule(std::chrono::seconds(60), [this, a_session, token] {
				if (session == a_session && scriptContext.calls.Token() == token && !scriptContext.calls.Busy())
					CloseScriptContext();
			});
		}
		requests.Complete(a_session, a_request, 1);
	}

	void BridgeController::RetireScriptContext(bool a_timeout, bool a_resume)
	{
		if (!scriptContext.lease)
			return;
		TaskScheduler::GetSingleton().Cancel(scriptContext.timer);
		scriptContext.executionDeadline.reset();
		WritePauseService::GetSingleton().Complete(std::exchange(scriptContext.pause, 0));
		if (scriptContext.stack) {
			for (const auto& [request, call] : scriptContext.admitted) {
				(void)call;
				NativeCallRequests().Cancel(NativeFacadeSession().Session(), scriptContext.stack->stackID, request);
			}
		}
		if (a_timeout) {
			quarantined.insert(scriptContext.mod);
			SKSE::log::error("External Papyrus host timed out: mod={} callback_running={}; late Papyrus execution may continue", scriptContext.mod, scriptContext.calls.Busy() || scriptContext.closing);
			WriteNotifications::Show(std::format("External MCM call timed out: {}\n\nThe host released its pause and execution lease. A started Papyrus callback may still finish later. No automatic retry or rollback was performed.", scriptContext.mod));
		}
		// Keep timed-out stack ownership as a tombstone: late UI calls must never
		// fall through to the Journal. The retained stack also prevents address reuse.
		if (!scriptContext.calls.Busy())
			NativeHostUI::DetachScriptStack(scriptContext.stack.get());
		const auto token = NativeFacadeSession().TokenForOwner(reinterpret_cast<std::uintptr_t>(scriptContext.script.get()));
		NativeFacadeSession().Close(token);
		registry.Native().ClearActive(scriptContext.script);
		if (scriptContext.borrowed) {
			// A failed borrowed callback must not leave the frontend holding its token.
			ClearHostedState();
		}
		const auto lease = scriptContext.lease;
		scriptContext.calls.Reset();
		scriptContext.script.reset();
		scriptContext.stack.reset();
		scriptContext.lease = scriptContext.timer = 0;
		scriptContext.closing = false;
		scriptContext.borrowed = false;
		scriptContext.admitted.clear();
		if (a_resume)
			EndExternalOperation(lease);
	}
}
