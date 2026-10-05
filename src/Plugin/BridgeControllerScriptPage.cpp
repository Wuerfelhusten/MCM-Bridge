#include "MCMBridge/Plugin/BridgeController.h"

#include "MCMBridge/Framework/FrameworkApi.h"
#include "MCMBridge/Localization/SnapshotLocalizer.h"

namespace MCMBridge
{
	void BridgeController::QueueScriptPage(std::uint64_t a_session, std::uint64_t a_revision, std::string a_modID, std::string a_page, bool a_opening)
	{
		if (session != a_session || !viewLoad.Current(a_revision))
			return;
		scriptPageRequest = ScriptPageRequest{ a_session, a_revision, std::move(a_modID), std::move(a_page), a_opening };
		QueueHostedDrive();
	}

	void BridgeController::ResolveScriptPage()
	{
		if (!scriptPageRequest)
			return;
		const auto request = std::exchange(scriptPageRequest, std::nullopt);
		if (request->session != session || !viewLoad.Current(request->revision) || !hostedScript ||
			!hostedScript->IsConfigOpen() || hostedDescriptor.stableID != request->mod)
			return;
		const auto names = hostedScript->ReadNavigationPages();
		if (!names) {
			viewLoad.Complete(request->revision, "Could not read navigation for the script-requested page.");
			return;
		}
		auto       snapshot = *snapshots.Get();
		const auto mod = std::ranges::find(snapshot.mods, request->mod, &MCMMod::stableID);
		if (mod == snapshot.mods.end())
			return;
		bool        changed{};
		const auto* selected = MergeScriptNavigation(*mod, *names, request->page, changed, request->opening);
		if (!selected) {
			viewLoad.Complete(request->revision, "The script-requested page is missing or ambiguous in current navigation.");
			return;
		}
		{
			const std::scoped_lock lock(hostedRequestMutex);
			if (requestedHostedModID != request->mod || requestedHostedPageID.empty() || !viewLoad.Remap(request->revision, selected->stableID))
				return;
			hostedPageRoute.Redirect(request->mod, requestedHostedPageID, selected->stableID);
			requestedHostedPageID = selected->stableID;
			navigationRecovery = {};
		}
		hostedReady = false;
		if (changed) {
			SnapshotLocalizer::Localize(*mod);
			snapshots.Publish(std::move(snapshot));
			FrameworkApi::GetSingleton().SynchronizeMCMs(snapshots.Get()->mods);
		}
		SKSE::log::info("Native external page selection: mod={} page=\"{}\" navigation_changed={}", request->mod, request->page, changed);
	}
}
