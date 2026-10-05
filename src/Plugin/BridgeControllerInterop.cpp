#include "MCMBridge/Plugin/BridgeController.h"

#include "MCMBridge/API/HostEvents.h"
#include "MCMBridge/Plugin/WritePauseService.h"

namespace MCMBridge
{
	MCMHostResult BridgeController::BeginExternalOperation(std::string_view a_owner, std::uint64_t& a_leaseToken)
	{
		a_leaseToken = 0;
		if (!sessionReady.load())
			return MCM_HOST_UNAVAILABLE;
		if (a_owner.empty())
			return MCM_HOST_INVALID_ARGUMENT;
		const bool        busy = refreshing || activeScan || activeWrite || activeHelp || activeHostedPage || activeHostedClose || IsClassicMCMActive();
		ExternalAdmission result;
		bool              closeHosted{};
		{
			const std::scoped_lock lock(externalOperationMutex);
			result = externalOperation.Begin(a_owner, busy || hostedScript, WritePauseService::GetSingleton().Pending() != 0, a_leaseToken);
			closeHosted = !busy && hostedScript && externalOperation.IsWaitingFor(a_owner);
		}
		if (result != ExternalAdmission::kReady) {
			if (result == ExternalAdmission::kBusy && closeHosted)
				CloseHostedSession();
			return result == ExternalAdmission::kBusy ? MCM_HOST_BUSY : MCM_HOST_UNAVAILABLE;
		}
		SKSE::log::info("Granted MCM operation lease {} to {}", a_leaseToken, a_owner);
		return MCM_HOST_OK;
	}

	MCMHostResult BridgeController::EndExternalOperation(std::uint64_t a_leaseToken)
	{
		{
			const std::scoped_lock lock(externalOperationMutex);
			if (!externalOperation.End(a_leaseToken))
				return MCM_HOST_UNAVAILABLE;
		}
		SKSE::log::info("Released MCM operation lease {}", a_leaseToken);
		if (refreshRequested.exchange(false))
			RefreshOnGameThread(false);
		else if (registryCheckPending.load())
			QueueRegistryCheck();
		else {
			ProcessWrites();
			DriveHostedPage();
		}
		return MCM_HOST_OK;
	}

	MCMHostResult BridgeController::CancelExternalOperation(std::string_view a_owner)
	{
		{
			const std::scoped_lock lock(externalOperationMutex);
			if (!externalOperation.Cancel(a_owner))
				return MCM_HOST_UNAVAILABLE;
			if (externalOperation.BlocksNormalWork())
				return MCM_HOST_OK;
		}
		if (refreshRequested.exchange(false))
			RefreshOnGameThread(false);
		else {
			ProcessWrites();
			DriveHostedPage();
		}
		return MCM_HOST_OK;
	}

	bool BridgeController::ExternalOperationBlocked() const
	{
		const std::scoped_lock lock(externalOperationMutex);
		return externalOperation.BlocksNormalWork();
	}

	bool BridgeController::AllowsRefresh() const
	{
		return !ExternalOperationBlocked();
	}

	void BridgeController::BeginCaptureSession()
	{
		if (captureSessionActive) {
			return;
		}
		captureSessionActive = true;
		++captureSessionID;
		if (captureSessionID == 0) {
			captureSessionID = 1;
		}
		HostEvents::GetSingleton().Session(MCM_HOST_SESSION_BEGIN, captureSessionID);
	}

	void BridgeController::UpdateCaptureSession()
	{
		bool requested;
		{
			const std::scoped_lock lock(hostedRequestMutex);
			requested = !requestedHostedModID.empty() && !requestedHostedPageID.empty();
		}
		if (requested)
			BeginCaptureSession();
		else if (WritePauseService::GetSingleton().Pending() == 0)
			EndCaptureSession();
	}

	void BridgeController::EndCaptureSession()
	{
		if (!captureSessionActive) {
			return;
		}
		HostEvents::GetSingleton().Session(MCM_HOST_SESSION_END, captureSessionID);
		captureSessionActive = false;
	}
}
