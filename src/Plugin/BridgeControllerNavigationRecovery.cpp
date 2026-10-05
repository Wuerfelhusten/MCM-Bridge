#include "MCMBridge/Plugin/BridgeController.h"

#include "MCMBridge/Framework/FrameworkApi.h"
#include "MCMBridge/Localization/SnapshotLocalizer.h"

namespace MCMBridge
{
	void BridgeController::RetryHostedPage()
	{
		{
			const std::scoped_lock lock(hostedRequestMutex);
			navigationRecovery = {};
		}
		viewLoad.Invalidate();
		QueueHostedDrive();
	}

	void BridgeController::RecoverHostedNavigation(std::uint64_t a_revision, const std::string& a_modID, const std::string& a_pageID)
	{
		if (!viewLoad.Current(a_revision)) {
			QueueHostedDrive();
			return;
		}
		bool retry{};
		{
			const std::scoped_lock lock(hostedRequestMutex);
			retry = navigationRecovery.Begin();
		}
		if (!retry) {
			viewLoad.Complete(a_revision, "MCM navigation changed again. Retry when the menu has finished updating.");
			CloseHostedSession();
			return;
		}
		hostedReady = false;
		const auto  names = hostedScript ? hostedScript->ReadNavigationPages() : std::nullopt;
		auto        snapshot = *snapshots.Get();
		auto        mod = std::ranges::find(snapshot.mods, a_modID, &MCMMod::stableID);
		std::string rawName;
		bool        hasSelection{};
		if (mod != snapshot.mods.end()) {
			const auto old = std::ranges::find(mod->pages, a_pageID, &MCMPage::stableID);
			if (old != mod->pages.end()) {
				rawName = old->rawName;
				hasSelection = true;
			}
		}
		if (!names || mod == snapshot.mods.end()) {
			{
				const std::scoped_lock lock(hostedRequestMutex);
				navigationRecovery.blocked = true;
			}
			viewLoad.Complete(a_revision, "MCM navigation is unavailable. The previous list was retained; retry to load it again.");
			CloseHostedSession();
			return;
		}
		const auto changed = MergeNavigation(*mod, *names);
		SnapshotLocalizer::Localize(*mod);
		const auto  selected = hasSelection ? ResolveNavigationSelection(*mod, a_pageID, rawName) : nullptr;
		std::string selection = selected ? selected->stableID : std::string{};
		bool        stale{};
		{
			const std::scoped_lock lock(hostedRequestMutex);
			stale = requestedHostedModID != a_modID || requestedHostedPageID != a_pageID || !viewLoad.Current(a_revision);
			if (!stale) {
				if (selected && hostedPageRoute.Remap(viewLoad, a_revision, a_modID, a_pageID, selection)) {
					requestedHostedPageID = selection;
				} else if (!selected) {
					navigationRecovery.blocked = true;
					viewLoad.Complete(a_revision, "The selected MCM page no longer exists. Select another page.");
				} else {
					stale = true;
				}
			}
		}
		if (stale) {
			QueueHostedDrive();
			return;
		}
		if (changed) {
			SKSE::log::info("Targeted MCM navigation refresh: mod={} pages={}", a_modID, mod->pages.size());
			snapshots.Publish(std::move(snapshot));
			FrameworkApi::GetSingleton().SynchronizeMCMs(snapshots.Get()->mods);
		}
		if (!selected) {
			CloseHostedSession();
			return;
		}
		QueueHostedDrive();
	}
}
