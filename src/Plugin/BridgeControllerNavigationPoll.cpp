#include "MCMBridge/Plugin/BridgeController.h"

#include "MCMBridge/Core/NavigationMerge.h"
#include "MCMBridge/Framework/FrameworkApi.h"
#include "MCMBridge/Localization/SnapshotLocalizer.h"

namespace MCMBridge
{
	void BridgeController::QueueNavigationPoll()
	{
		if (!frameworkViewOpen.load() || !sessionReady.load())
			return;
		const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::steady_clock::now().time_since_epoch())
		                     .count();
		if (now < nextNavigationPoll.load() || navigationPollQueued.exchange(true))
			return;
		nextNavigationPoll.store(now + 500);
		const auto epoch = navigationEpoch.load();
		if (auto* tasks = SKSE::GetTaskInterface()) {
			tasks->AddTask([this, epoch] {
				navigationPollQueued.store(false);
				if (epoch == navigationEpoch.load() && frameworkViewOpen.load())
					PollNavigation();
			});
		} else {
			navigationPollQueued.store(false);
		}
	}

	void BridgeController::PollNavigation()
	{
		if (!sessionReady.load() || refreshing || activeScan || activeWrite || activeHelp ||
			activeHostedPage || activeHostedClose || ExternalOperationBlocked() || IsClassicMCMActive())
			return;

		auto snapshot = *snapshots.Get();
		bool changed = false;
		bool hostedChanged = false;
		for (auto& mod : snapshot.mods) {
			if (quarantined.contains(mod.stableID))
				continue;
			const auto live = std::ranges::find_if(liveEntries, [&](const auto& a_entry) {
				return a_entry.descriptor.stableID == mod.stableID;
			});
			if (live == liveEntries.end() || !live->adapter)
				continue;
			std::optional<std::vector<std::string>> names;
			if (IsNativeHost()) {
				names = hostedScript && hostedDescriptor.stableID == mod.stableID ?
				            hostedScript->ReadNavigationPages() :
				            live->adapter->ReadRegisteredPages();
			} else if (const auto script = live->adapter->CreateSession()) {
				names = script->ReadNavigationPages();
			}
			std::string selectedID;
			std::string selectedName;
			bool        hasSelection{};
			{
				const std::scoped_lock lock(hostedRequestMutex);
				if (requestedHostedModID == mod.stableID) {
					selectedID = requestedHostedPageID;
					const auto selected = std::ranges::find(mod.pages, selectedID, &MCMPage::stableID);
					if (selected != mod.pages.end()) {
						selectedName = selected->rawName;
						hasSelection = true;
					}
				}
			}
			if (!names || !MergeNavigation(mod, *names))
				continue;
			if (hasSelection) {
				const auto             selection = ResolveNavigationSelection(mod, selectedID, selectedName);
				const std::scoped_lock lock(hostedRequestMutex);
				if (requestedHostedModID == mod.stableID && requestedHostedPageID == selectedID) {
					if (selection)
						requestedHostedPageID = selection->stableID;
					else {
						navigationRecovery.blocked = true;
						viewLoad.Complete(viewLoad.Request(mod.stableID, selectedID), "The selected MCM page no longer exists. Select another page.");
					}
				}
			}
			SnapshotLocalizer::Localize(mod);
			changed = true;
			hostedChanged = hostedChanged || mod.stableID == hostedDescriptor.stableID;
			SKSE::log::info("MCM navigation updated: mod={} pages={}", mod.stableID, mod.pages.size());
		}
		if (!changed)
			return;
		snapshots.Publish(std::move(snapshot));
		FrameworkApi::GetSingleton().SynchronizeMCMs(snapshots.Get()->mods);
		if (hostedChanged) {
			hostedReady = false;
			viewLoad.Invalidate(true);
			QueueHostedDrive();
		}
	}
}
