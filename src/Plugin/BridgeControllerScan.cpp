#include "MCMBridge/Framework/FrameworkApi.h"
#include "MCMBridge/Localization/SnapshotLocalizer.h"
#include "MCMBridge/Papyrus/NativeHostPreflight.h"
#include "MCMBridge/Plugin/BridgeController.h"
#include "MCMBridge/Plugin/TaskScheduler.h"

namespace MCMBridge
{
	void BridgeController::BeginScan(std::vector<LiveMCM> a_entries)
	{
#ifdef MCM_BRIDGE_NATIVE_HOST_PREFLIGHT
		RunNativeRegistryPreflight(a_entries, session);
#endif
		const auto fullRequested = fullRefreshRequested.exchange(false);
		// Registry discovery needs navigation; selected pages are built by the hosted session.
		const bool navigationOnly = !fullRequested;
		const bool retainHosted = navigationOnly && hostedScript && hostedReady &&
		                          viewLoad.Ready(hostedDescriptor.stableID, hostedPageID);
		// A full refresh or a newer selection may arrive after the caller checked
		// whether navigation can be read without releasing the current session.
		if (hostedScript && !retainHosted) {
			if (fullRequested)
				fullRefreshRequested.store(true);
			CloseHostedSession([this, entries = std::move(a_entries)]() mutable { BeginScan(std::move(entries)); });
			return;
		}
		if (!retainHosted)
			viewLoad.Invalidate();
		SKSE::log::info("Scanning {} registered MCM scripts: navigation_only={}", a_entries.size(), navigationOnly);
		refreshing = true;
		liveEntries = std::move(a_entries);
		scanIndex = 0;
		const auto                 currentSnapshot = snapshots.Get();
		std::vector<MCMDescriptor> descriptors;
		for (const auto& entry : liveEntries) descriptors.push_back(entry.descriptor);
		pendingSnapshot = PrepareRegistryRefresh(*currentSnapshot, descriptors);
		if (navigationOnly) {
			ReadNativeNavigation();
			return;
		}
		for (auto& mod : pendingSnapshot.mods) SnapshotLocalizer::Localize(mod);
		snapshots.Publish(pendingSnapshot);
		FrameworkApi::GetSingleton().SynchronizeMCMs(pendingSnapshot.mods);
		ScanNext(session);
	}

	void BridgeController::ScanNext(std::uint64_t a_session)
	{
		if (a_session != session) {
			return;
		}
		while (scanIndex < liveEntries.size() && quarantined.contains(liveEntries[scanIndex].descriptor.stableID)) {
			++scanIndex;
		}
		if (scanIndex >= liveEntries.size()) {
			FinishScan(a_session);
			return;
		}

		const auto currentIndex = scanIndex++;
		activeScan = std::make_shared<ClassicScanOperation>(
			liveEntries[currentIndex].descriptor,
			liveEntries[currentIndex].adapter->CreateSession(),
			menuResolver,
			ClassicScanOperation::BusyCheck{},
			[this, a_session, currentIndex](Result<MCMMod> a_result) {
				if (a_session != session) {
					return;
				}
				activeScan.reset();
				if (a_result) {
					for (const auto& page : a_result->pages)
						ObserveCustomContent(*a_result, page, CustomContentOrigin::kScan);
					pendingSnapshot.mods[currentIndex] = a_result->backend == MCMBackendKind::kMCMHelper ?
				                                             MergeHelper(std::move(*a_result)) :
				                                             std::move(*a_result);
					SnapshotLocalizer::Localize(pendingSnapshot.mods[currentIndex]);
					FrameworkApi::GetSingleton().SynchronizeMCMs(pendingSnapshot.mods);
				} else {
					const auto& descriptor = liveEntries[currentIndex].descriptor;
					SKSE::log::warn("MCM scan failed for {} ({} / {}): {}", descriptor.stableID, descriptor.displayName, descriptor.scriptName, a_result.error().message);
					pendingSnapshot.diagnostics.push_back({ DiagnosticSeverity::kWarning, liveEntries[currentIndex].descriptor.stableID, a_result.error().message });
					if (a_result.error().code == BridgeErrorCode::kTimedOut) {
						quarantined.insert(liveEntries[currentIndex].descriptor.stableID);
						SKSE::log::warn("Quarantined MCM {} for the current game session", liveEntries[currentIndex].descriptor.stableID);
					}
				}
				snapshots.Publish(pendingSnapshot);
				ScanNext(a_session);
			},
			TaskScheduler::GetSingleton());
		const auto operation = activeScan;
		operation->Start();
	}

	void BridgeController::FinishScan(std::uint64_t a_session)
	{
		if (a_session != session) {
			return;
		}
		refreshing = false;
		pendingSnapshot.refreshing = false;
		for (auto& mod : pendingSnapshot.mods) {
			SnapshotLocalizer::Localize(mod);
		}
		FrameworkApi::GetSingleton().SynchronizeMCMs(pendingSnapshot.mods);
		SKSE::log::info("MCM scan completed with {} mods and {} diagnostics", pendingSnapshot.mods.size(), pendingSnapshot.diagnostics.size());
		snapshots.Publish(std::move(pendingSnapshot));
		if (ExternalOperationBlocked()) {
			return;
		}
		if (refreshRequested.exchange(false)) {
			RefreshOnGameThread(false);
		} else if (registryCheckPending.load()) {
			QueueRegistryCheck();
		} else {
			ProcessWrites();
			DriveHostedPage();
		}
	}
}
