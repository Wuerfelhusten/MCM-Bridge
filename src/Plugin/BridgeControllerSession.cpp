#include "MCMBridge/Plugin/BridgeController.h"
#include "MCMBridge/Plugin/BridgeSettingsService.h"

#include "MCMBridge/Framework/FrameworkApi.h"
#include "MCMBridge/Papyrus/NativeFacade.h"
#include "MCMBridge/Plugin/TaskScheduler.h"
#include "MCMBridge/Plugin/WritePauseService.h"
#include "MCMBridge/UI/MessageDialog.h"
#include "MCMBridge/UI/QuickOpenWindow.h"
#include "MCMBridge/UI/WriteNotifications.h"
#include "MCMBridge/Write/WriteTiming.h"

namespace MCMBridge
{
	void BridgeController::StartSession(std::string a_reason, bool a_discover)
	{
		SKSE::log::info("Starting MCM Bridge session: {}", a_reason);
		{
			const std::scoped_lock lock(hostedRequestMutex);
			if (customVisits.count || customVisits.scanResults || customVisits.restoreResults)
				SKSE::log::info("Native custom content session summary: user_openings={} scan_results={} restore_results={}",
					customVisits.count, customVisits.scanResults, customVisits.restoreResults);
			customVisits = {};
		}
		++session;
		RetireScriptContext(false, false);
		activeRecordingID = 0;
		if (directContext.calls)
			directContext.calls->Invalidate();
		directContext = {};
		scriptPageRequest.reset();
		QuickOpenWindow::Close();
		NativeFacadeSession().Reset(session);
		NativeRegistryRequests().Reset(session);
		NativeCallRequests().Reset(session);
		++navigationEpoch;
		viewLoad.Invalidate();
		sessionReady.store(false);
		refreshRequested.store(false);
		fullRefreshRequested.store(false);
		writeRetryScheduled = false;
		WritePauseService::GetSingleton().Reset();
		WriteNotifications::Reset();
		if (activeWriteTiming) {
			activeWriteTiming->Finish(WriteTimingOutcome::kCancelled, a_reason);
			activeWriteTiming.reset();
		}
		EndCaptureSession();
		{
			const std::scoped_lock lock(externalOperationMutex);
			externalOperation.Reset();
		}
		if (activeScan) {
			activeScan->Cancel();
			activeScan.reset();
		}
		if (activeWrite) {
			activeWrite->Cancel();
			activeWrite.reset();
		}
		if (activeHelp) {
			auto operation = std::move(activeHelp);
			operation->Cancel();
		}
		AbandonHostedSession(true);
		writes.Clear();
		MessageDialog::Cancel();
		liveEntries.clear();
		BridgeSettingsService::GetSingleton().SetProviderAliases({});
		quarantined.clear();
		{
			const std::scoped_lock lock(helpMutex);
			pendingHelp.clear();
			resolvedHelp.clear();
		}
		registryIDs.clear();
		registry.Reset(true);
		registryCheckPending.store(false);
		frontendInvalidationQueued.store(false);
		refreshing = false;
		snapshots.Reset(std::move(a_reason));
		if (!a_discover)
			return;
		if (!FrameworkApi::GetSingleton().IsAvailable()) {
			SKSE::log::error("MCM Bridge is inert because SKSE Menu Framework is unavailable");
			return;
		}
		sessionReady.store(true);
		const auto native = registry.PrepareNative(session);
		if (!native) {
			const bool waiting = native.error().code == BridgeErrorCode::kBusy;
			SKSE::log::info("Native host session admission: {}", native.error().message);
			auto snapshot = *snapshots.Get();
			snapshot.diagnostics.push_back({ waiting ? DiagnosticSeverity::kInfo : DiagnosticSeverity::kError, "native-host", native.error().message });
			snapshots.Publish(std::move(snapshot));
			return;
		}
		RequestRefresh(true);
	}
}
