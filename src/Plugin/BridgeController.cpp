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
		if (!a_navigationOnly)
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

	void BridgeController::NotifyRegistryEvent()
	{
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

	bool BridgeController::IsRegistryAvailable() const
	{
		return registry.IsAvailable();
	}

	void BridgeController::ObserveMenuOptions(
		std::string_view         a_menuName,
		std::string_view         a_target,
		std::vector<std::string> a_options)
	{
		menuResolver.ObserveInvokeStringArray(a_menuName, a_target, std::move(a_options));
	}

	void BridgeController::RefreshOnGameThread(bool a_registryCheck)
	{
		if (!GameSessionEvents::CanUseGame()) {
			if (!refreshRequested.exchange(true)) {
				const auto operationSession = session;
				TaskScheduler::GetSingleton().After(busyRetryDelay, [operationSession, a_registryCheck] {
					auto& controller = GetSingleton();
					if (controller.session != operationSession)
						return;
					controller.refreshRequested.store(false);
					controller.RefreshOnGameThread(a_registryCheck);
				});
			}
			return;
		}
		if (GameSessionEvents::TryStartSession())
			return;
		if (!sessionReady.load())
			return;
		if (!AllowsRefresh()) {
			if (a_registryCheck) {
				registryCheckPending.store(true);
			} else {
				refreshRequested.store(true);
			}
			return;
		}
		if (refreshing || activeScan || activeWrite || activeHelp || activeHostedPage || activeHostedClose) {
			if (a_registryCheck) {
				registryCheckPending.store(true);
			} else {
				refreshRequested.store(true);
			}
			return;
		}
		if (!a_registryCheck) {
			registryIDs.clear();
			registry.Reset();
		}

		auto entries = registry.ReadLive();
		if (!entries) {
			SKSE::log::warn("MCM registry discovery failed: {}", entries.error().message);
			MCMSnapshot snapshot = *snapshots.Get();
			snapshot.refreshing = false;
			snapshot.diagnostics.push_back({ DiagnosticSeverity::kError, "discovery", entries.error().message });
			snapshots.Publish(std::move(snapshot));
			return;
		}

		std::ranges::sort(*entries, {}, [](const auto& a_entry) { return a_entry.descriptor.stableID; });
		std::vector<std::string> ids;
		ids.reserve(entries->size());
		for (const auto& entry : *entries) {
			ids.push_back(entry.descriptor.stableID);
		}

		const bool unchanged = ids == registryIDs;
		registryIDs = std::move(ids);
		if (a_registryCheck && unchanged) {
			liveEntries = std::move(*entries);
			FrameworkApi::GetSingleton().SynchronizeMCMs(snapshots.Get()->mods);
			ProcessWrites();
			DriveHostedPage();
			return;
		}

		if (!fullRefreshRequested.load() && hostedScript && hostedReady &&
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
