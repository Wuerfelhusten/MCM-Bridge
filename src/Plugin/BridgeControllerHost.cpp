#include "MCMBridge/Plugin/BridgeController.h"

#include "MCMBridge/Papyrus/HostCallArguments.h"
#include "MCMBridge/Plugin/TaskScheduler.h"
#include "MCMBridge/Plugin/WritePauseService.h"

namespace
{
	MCMHostResult Convert(MCMBridge::HostCallStatus a_status)
	{
		using Status = MCMBridge::HostCallStatus;
		switch (a_status) {
		case Status::kCompleted:
			return MCM_HOST_OK;
		case Status::kDispatchFailed:
			return MCM_HOST_DISPATCH_FAILED;
		case Status::kInvalidData:
		case Status::kControlUnavailable:
			return MCM_HOST_INVALID_DATA;
		case Status::kCancelled:
			return MCM_HOST_CANCELLED;
		case Status::kTimedOut:
			return MCM_HOST_TIMED_OUT;
		case Status::kSessionInvalidated:
			return MCM_HOST_SESSION_INVALIDATED;
		}
		return MCM_HOST_UNAVAILABLE;
	}
}

namespace MCMBridge
{
	MCMHostResult BridgeController::BeginHostContext(std::string_view a_owner, bool a_restore, MCMHostContext& a_context)
	{
		a_context = 0;
		if (!IsSessionReady() || !registry.Native().IsAvailable())
			return MCM_HOST_UNAVAILABLE;
		if (directContext.id)
			return MCM_HOST_BUSY;
		if (a_owner.empty())
			return MCM_HOST_INVALID_ARGUMENT;
		if (!nextHostContext)
			return MCM_HOST_UNAVAILABLE;
		std::uint64_t lease{};
		// Direct calls require exclusive execution, not the managed restore queue.
		const auto result = BeginExternalOperation(a_owner, lease);
		if (result != MCM_HOST_OK && result != MCM_HOST_BUSY) {
			CancelExternalOperation(a_owner);
			return MCM_HOST_UNAVAILABLE;
		}
		// Reserve admission while an existing UI callback or CloseConfig drains.
		// The first invoke waits for execution ownership instead of losing the start.
		directContext.id = a_context = nextHostContext++;
		directContext.lease = lease;
		directContext.owner = a_owner;
		directContext.started = std::chrono::steady_clock::now();
		directContext.restore = a_restore;
		SKSE::log::info("Direct host context reserved: context={} owner={} restore={}", a_context, a_owner, a_restore);
		return MCM_HOST_OK;
	}

	MCMHostResult BridgeController::InvokeHost(MCMHostContext a_context, const MCMHostCall& a_call, MCMHostCompletion a_completion, void* a_user)
	{
		if (!a_context || directContext.id != a_context || directContext.ended || directContext.cancelled || !IsSessionReady())
			return MCM_HOST_UNAVAILABLE;
		if (!a_completion)
			return MCM_HOST_INVALID_ARGUMENT;
		auto call = DecodeHostCall(a_call);
		if (!call)
			return MCM_HOST_INVALID_ARGUMENT;
		if (!directContext.lease) {
			const auto result = BeginExternalOperation(directContext.owner, directContext.lease);
			if (result != MCM_HOST_OK)
				return result == MCM_HOST_BUSY ? MCM_HOST_BUSY : MCM_HOST_UNAVAILABLE;
		}
		if (directContext.calls && directContext.calls->Busy())
			return MCM_HOST_BUSY;
		if (directContext.calls && directContext.modID == a_call.mcm_id &&
			(directContext.calls->Stopped() || quarantined.contains(directContext.stableID)))
			return MCM_HOST_UNAVAILABLE;
		if (directContext.modID != a_call.mcm_id || !directContext.calls) {
			if (directContext.script && directContext.script->IsConfigOpen())
				return MCM_HOST_INVALID_ARGUMENT;
			if (call->method != ClassicMethod::kOpenConfig)
				return MCM_HOST_INVALID_ARGUMENT;
			auto entries = registry.Native().ReadLive();
			if (!entries)
				return MCM_HOST_UNAVAILABLE;
			const LiveMCM* match{};
			for (const auto& entry : *entries) {
				if (entry.descriptor.interopID == a_call.mcm_id) {
					if (match)
						return MCM_HOST_INVALID_DATA;
					match = &entry;
				}
			}
			if (!match || !match->adapter || !match->adapter->IsValid() || quarantined.contains(match->descriptor.stableID))
				return MCM_HOST_UNAVAILABLE;
			auto script = match->adapter->CreateSession();
			if (!script)
				return MCM_HOST_UNAVAILABLE;
			directContext.calls.reset();
			directContext.script = std::move(script);
			directContext.calls = std::make_shared<HostCallSession>(directContext.script, TaskScheduler::GetSingleton());
			directContext.modID = a_call.mcm_id;
			directContext.stableID = match->descriptor.stableID;
		}
		if (directContext.restore && RequiresHostWritePause(call->method)) {
			auto& pause = WritePauseService::GetSingleton();
			if (!directContext.pause)
				directContext.pause = pause.Reserve();
			const auto ready = pause.Begin(directContext.pause);
			if (!ready)
				return MCM_HOST_UNAVAILABLE;
			if (!*ready)
				return MCM_HOST_BUSY;
		} else {
			// Read/navigation callbacks and client-side activation waits need game time.
			WritePauseService::GetSingleton().Complete(std::exchange(directContext.pause, 0));
		}
		directContext.acceptConfirmation = a_call.accept_confirmation != 0;
		directContext.confirmationDeclined = false;
		const auto generation = session;
		const auto method = call->method;
		const auto request = *call;
		directContext.lastCall.reset();
		const auto started = std::chrono::steady_clock::now();
		SKSE::log::debug("Direct host call submitted: context={} mod={} method={}", a_context, directContext.modID, ClassicMethodName(method));
		const auto accepted = directContext.calls->Submit(std::move(*call), [this, a_context, generation, method, request, started, a_completion, a_user](HostCallStatus a_status) {
			const bool current = session == generation && directContext.id == a_context;
			const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
			if (current) {
				// Release before notifying the client. Its next refresh must not inherit
				// the previous mutation's pause; exclusive execution ownership remains.
				WritePauseService::GetSingleton().Complete(std::exchange(directContext.pause, 0));
				++directContext.completedCalls.at(static_cast<std::size_t>(method));
				directContext.failedCalls += a_status != HostCallStatus::kCompleted ? 1U : 0U;
				directContext.callMilliseconds += elapsed;
			}
			if (current && a_status == HostCallStatus::kCompleted)
				directContext.lastCall = request;
			const auto declined = current && directContext.confirmationDeclined;
			if (current && a_status == HostCallStatus::kTimedOut) {
				quarantined.insert(directContext.stableID);
				WritePauseService::GetSingleton().Complete(std::exchange(directContext.pause, 0));
			}
			if (current && (a_status == HostCallStatus::kInvalidData || a_status == HostCallStatus::kDispatchFailed)) {
				quarantined.insert(directContext.stableID);
				directContext.script->RetireExecution();
			}
			if (current && directContext.ended)
				CloseHostContext();
			const auto result = current ? Convert(a_status) : MCM_HOST_SESSION_INVALIDATED;
			if (method == ClassicMethod::kSelectOption && (elapsed >= 250 || result != MCM_HOST_OK))
				SKSE::log::info("Direct host selection completed: context={} option={} result={} confirmation_declined={} elapsed_ms={:.3f}",
					a_context, request.integer, result, declined, elapsed);
			SKSE::log::debug("Direct host call completed: context={} method={} result={} confirmation_declined={} elapsed_ms={:.3f}", a_context, ClassicMethodName(method), result, declined, elapsed);
			try {
				a_completion(a_user, result, declined ? 1U : 0U);
			} catch (...) {
				SKSE::log::error("Direct host client threw from completion: context={}", a_context);
			} }, std::chrono::milliseconds(a_call.timeout_ms));
		if (!accepted)
			WritePauseService::GetSingleton().Complete(std::exchange(directContext.pause, 0));
		return accepted ? MCM_HOST_OK : MCM_HOST_UNAVAILABLE;
	}

	MCMHostResult BridgeController::EndHostContext(MCMHostContext a_context, bool a_cancel)
	{
		if (!a_context || a_context != directContext.id)
			return MCM_HOST_UNAVAILABLE;
		directContext.cancelled = true;
		directContext.ended |= !a_cancel;
		if (directContext.calls && !directContext.closing)
			directContext.calls->Cancel();
		if (directContext.ended && (!directContext.calls || !directContext.calls->Busy()))
			CloseHostContext();
		return MCM_HOST_OK;
	}

	void BridgeController::ReleaseHostContext()
	{
		const auto&   calls = directContext.completedCalls;
		std::uint64_t count{};
		for (std::size_t index = 0; index < calls.size(); ++index) {
			count += calls[index];
			if (calls[index])
				SKSE::log::info("Direct host call count: context={} method={} count={}", directContext.id,
					ClassicMethodName(static_cast<ClassicMethod>(index)), calls[index]);
		}
		const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - directContext.started).count();
		SKSE::log::info("Direct host context finished: context={} owner={} restore={} calls={} failed={} call_ms={:.3f} total_ms={:.3f}",
			directContext.id, directContext.owner, directContext.restore, count, directContext.failedCalls, directContext.callMilliseconds, elapsed);
		// Flush once per context so a normal game exit cannot lose its summary.
		spdlog::default_logger()->flush();
		const auto lease = directContext.lease;
		const auto owner = directContext.owner;
		WritePauseService::GetSingleton().Complete(directContext.pause);
		directContext = {};
		if (lease)
			EndExternalOperation(lease);
		else
			CancelExternalOperation(owner);
	}

	void BridgeController::CloseHostContext()
	{
		if (!directContext.calls || !directContext.script || !directContext.script->IsConfigOpen()) {
			ReleaseHostContext();
			return;
		}
		if (directContext.closing)
			return;
		directContext.closing = true;
		directContext.acceptConfirmation = false;
		const auto lease = directContext.id;
		const auto generation = session;
		if (!directContext.calls->Close([this, lease, generation](HostCallStatus a_status) {
				if (session != generation || directContext.id != lease)
					return;
				if (a_status != HostCallStatus::kCompleted)
					quarantined.insert(directContext.stableID);
				ReleaseHostContext();
			},
				std::chrono::seconds(30))) {
			quarantined.insert(directContext.stableID);
			ReleaseHostContext();
		}
	}

	std::optional<bool> BridgeController::HostMessageResponse(bool a_confirmation)
	{
		if (!directContext.calls || !directContext.calls->Busy())
			return std::nullopt;
		const bool accepted = !a_confirmation || directContext.acceptConfirmation;
		directContext.confirmationDeclined |= !accepted;
		return accepted;
	}
}
