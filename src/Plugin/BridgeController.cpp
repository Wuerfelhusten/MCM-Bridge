#include "MCMBridge/Plugin/BridgeController.h"
#include "MCMBridge/Plugin/BridgeSettingsService.h"

#include "MCMBridge/Core/MCMHelperMerge.h"
#include "MCMBridge/Core/MCMHelperParser.h"
#include "MCMBridge/Framework/FrameworkApi.h"
#include "MCMBridge/Localization/SnapshotLocalizer.h"
#include "MCMBridge/Plugin/GameSessionEvents.h"
#include "MCMBridge/Plugin/TaskScheduler.h"

#include <algorithm>

namespace
{
	constexpr auto busyRetryDelay = std::chrono::seconds(1);
}

namespace MCMBridge
{
	BridgeController& BridgeController::GetSingleton()
	{
		static BridgeController singleton;
		return singleton;
	}

	void BridgeController::RequestRefresh(bool a_navigationOnly)
	{
		if (!registry.IsAvailable() || !FrameworkApi::GetSingleton().IsAvailable()) {
			return;
		}
		if (a_navigationOnly)
			navigationRefreshRequested.store(true);
		else
			fullRefreshRequested.store(true);
		if (refreshRequested.exchange(true)) {
			return;
		}
		if (auto* tasks = SKSE::GetTaskInterface()) {
			tasks->AddTask([] {
				auto& controller = GetSingleton();
				controller.refreshRequested.store(false);
				controller.RefreshOnGameThread(false);
			});
		}
	}

	void BridgeController::NotifyRegistryEvent(bool a_reset)
	{
		if (a_reset) {
			registryResetRequested.store(true);
		}
		registryCheckPending.store(true);
		QueueRegistryCheck();
	}

	void BridgeController::QueueRegistryCheck()
	{
		if (registryEventQueued.exchange(true)) {
			return;
		}
		if (auto* tasks = SKSE::GetTaskInterface()) {
			tasks->AddTask([] {
				auto& controller = GetSingleton();
				controller.registryEventQueued.store(false);
				if (!controller.registryCheckPending.exchange(false)) {
					return;
				}
				controller.registry.Reset();
				if (controller.registryResetRequested.exchange(false) && !controller.registry.UsesNative()) {
					controller.StartSession("MCM registry reset", controller.sessionReady.load());
					return;
				}
				controller.RefreshOnGameThread(true);
			});
		} else {
			registryEventQueued.store(false);
		}
	}

	void BridgeController::RetryFailed()
	{
		if (auto* tasks = SKSE::GetTaskInterface()) {
			tasks->AddTask([] {
				auto& controller = GetSingleton();
				controller.quarantined.clear();
				SKSE::log::info("Retrying quarantined MCM scripts");
				controller.RequestRefresh();
			});
		}
	}

	std::shared_ptr<const MCMSnapshot> BridgeController::Snapshot() const
	{
		return snapshots.Get();
	}

	bool BridgeController::IsSkyUIAvailable() const
	{
		return registry.IsAvailable();
	}

	bool BridgeController::IsClassicMCMActive() const
	{
		if (IsNativeHost())
			return false;
		return originalMCMOpen.load() || registry.IsBusy();
	}

	void BridgeController::NotifyOriginalMCMState(bool a_open)
	{
		if (IsNativeHost())
			return;
		if (originalMCMOpen.exchange(a_open) == a_open)
			return;
		const auto operationSession = session;
		if (auto* tasks = SKSE::GetTaskInterface()) {
			tasks->AddTask([a_open, operationSession] {
				GetSingleton().HandleOriginalMCMState(a_open, operationSession);
			});
		}
	}

	void BridgeController::HandleOriginalMCMState(bool a_open, std::uint64_t a_session)
	{
		if (a_session != session)
			return;
		if (a_open) {
			if (activeScan) {
				auto operation = activeScan;
				operation->YieldToFrontend();
			}
			if (activeWrite) {
				auto operation = activeWrite;
				operation->YieldToFrontend();
			}
			if (activeHostedPage) {
				auto operation = activeHostedPage;
				operation->YieldToFrontend();
			}
			if (activeHelp) {
				auto operation = activeHelp;
				operation->Cancel();
			}
			return;
		}
		scanPausedForOriginalMCM = false;
		refreshRequested.store(false);
		RequestRefresh();
	}

	void BridgeController::ObserveMenuOptions(
		std::string_view         a_menuName,
		std::string_view         a_target,
		std::vector<std::string> a_options)
	{
		menuResolver.ObserveInvokeStringArray(a_menuName, a_target, std::move(a_options));
	}

	void BridgeController::RefreshOnGameThread(bool a_stabilityCheck)
	{
		if (!GameSessionEvents::CanUseGame()) {
			if (!refreshRequested.exchange(true)) {
				const auto operationSession = session;
				TaskScheduler::GetSingleton().After(busyRetryDelay, [operationSession, a_stabilityCheck] {
					auto& controller = GetSingleton();
					if (controller.session != operationSession)
						return;
					controller.refreshRequested.store(false);
					controller.RefreshOnGameThread(a_stabilityCheck);
				});
			}
			return;
		}
		if (GameSessionEvents::TryStartSession())
			return;
		if (!sessionReady.load())
			return;
		if (!AllowsRefresh()) {
			if (a_stabilityCheck) {
				registryCheckPending.store(true);
			} else {
				refreshRequested.store(true);
			}
			return;
		}
		if (refreshing || activeScan || activeWrite || activeHelp || activeHostedPage || activeHostedClose) {
			if (a_stabilityCheck) {
				registryCheckPending.store(true);
			} else {
				refreshRequested.store(true);
			}
			return;
		}
		if (originalMCMOpen.load()) {
			scanPausedForOriginalMCM = true;
			refreshRequested.store(true);
			return;
		}
		if (registry.IsBusy()) {
			TaskScheduler::GetSingleton().After(busyRetryDelay, [a_stabilityCheck] {
				GetSingleton().RefreshOnGameThread(a_stabilityCheck);
			});
			return;
		}
		if (!a_stabilityCheck) {
			registrySettler.Reset();
			registry.Reset();
		}

		auto entries = registry.ReadLive();
		if (!entries) {
			if (entries.error().code == BridgeErrorCode::kBusy) {
				if (registryRetryScheduled)
					return;
				registryRetryScheduled = true;
				const auto operationSession = session;
				TaskScheduler::GetSingleton().After(std::chrono::milliseconds(100), [a_stabilityCheck, operationSession] {
					auto& controller = GetSingleton();
					if (controller.session != operationSession || !controller.registryRetryScheduled)
						return;
					controller.registryRetryScheduled = false;
					controller.RefreshOnGameThread(a_stabilityCheck);
				});
				return;
			}
			registryRetryScheduled = false;
			SKSE::log::warn("MCM registry discovery failed: {}", entries.error().message);
			MCMSnapshot snapshot = *snapshots.Get();
			snapshot.refreshing = false;
			snapshot.diagnostics.push_back({ DiagnosticSeverity::kError, "discovery", entries.error().message });
			snapshots.Publish(std::move(snapshot));
			const auto settle = registrySettler.Observe({});
			if (settle != RegistrySettleResult::kExpired) {
				ScheduleStabilityCheck(session);
			}
			return;
		}

		registryRetryScheduled = false;
		std::ranges::sort(*entries, {}, [](const auto& a_entry) { return a_entry.descriptor.stableID; });
		std::map<std::string, std::string, std::less<>> providerAliases;
		for (const auto& entry : *entries) {
			if (!entry.registryDisplayName.empty())
				providerAliases.emplace(entry.descriptor.stableID, entry.registryDisplayName);
		}
		BridgeSettingsService::GetSingleton().SetProviderAliases(std::move(providerAliases));
		std::vector<std::string> ids;
		ids.reserve(entries->size());
		for (const auto& entry : *entries) {
			ids.push_back(entry.descriptor.stableID);
		}

		const auto settle = registrySettler.Observe(ids);
		if (a_stabilityCheck && settle != RegistrySettleResult::kChanged) {
			liveEntries = std::move(*entries);
			FrameworkApi::GetSingleton().SynchronizeMCMs(snapshots.Get()->mods);
			if (!IsNativeHost() && registrySettler.ShouldContinue()) {
				ScheduleStabilityCheck(session);
			}
			ProcessWrites();
			DriveHostedPage();
			return;
		}

		if (IsNativeHost() && !fullRefreshRequested.load() && hostedScript && hostedReady &&
			hostedScript->IsConfigOpen() && viewLoad.Ready(hostedDescriptor.stableID, hostedPageID)) {
			const auto previous = std::ranges::find_if(liveEntries, [&](const auto& a_entry) {
				return a_entry.descriptor.stableID == hostedDescriptor.stableID;
			});
			const auto current = std::ranges::find_if(*entries, [&](const auto& a_entry) {
				return a_entry.descriptor.stableID == hostedDescriptor.stableID;
			});
			if (previous != liveEntries.end() && current != entries->end() && previous->adapter &&
				previous->adapter->IsValid() && current->adapter && current->adapter->IsValid()) {
				BeginScan(std::move(*entries));
				return;
			}
		}
		if (hostedScript) {
			CloseHostedSession([this, entries = std::move(*entries)]() mutable {
				BeginScan(std::move(entries));
			});
			return;
		}
		BeginScan(std::move(*entries));
	}

	std::optional<LiveMCM> BridgeController::FindLive(const SettingIdentity& a_identity) const
	{
		const auto found = std::ranges::find_if(liveEntries, [&](const auto& a_entry) {
			return a_entry.descriptor.ownerPlugin == a_identity.ownerPlugin &&
			       a_entry.descriptor.questFormID == a_identity.questFormID &&
			       a_entry.descriptor.scriptName == a_identity.scriptName;
		});
		return found != liveEntries.end() ? std::optional<LiveMCM>(*found) : std::nullopt;
	}

	MCMMod BridgeController::MergeHelper(MCMMod a_liveMod) const
	{
		const auto     modName = std::filesystem::path(a_liveMod.ownerPlugin).stem().string();
		const auto     directory = std::filesystem::path("Data/MCM/Config") / modName;
		MCMHelperPaths paths{
			.config = directory / "config.json",
			.defaults = directory / "settings.ini",
			.userSettings = std::filesystem::path("Data/MCM/Settings") / std::format("{}.ini", modName)
		};
		if (auto parsed = MCMHelperParser{}.Parse(paths)) {
			MergeMCMHelperMetadata(a_liveMod, *parsed);
		} else {
			SKSE::log::warn("Could not load MCM Helper metadata for {}: {}", modName, parsed.error().message);
		}
		return a_liveMod;
	}
}
