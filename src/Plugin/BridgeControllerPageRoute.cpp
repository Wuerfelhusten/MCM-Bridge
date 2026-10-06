#include "MCMBridge/Framework/FrameworkApi.h"
#include "MCMBridge/Plugin/BridgeController.h"
#include "MCMBridge/UI/QuickOpenWindow.h"

namespace MCMBridge
{
	std::optional<std::uint64_t> BridgeController::CaptureScriptView(std::string_view a_modID) const
	{
		const std::scoped_lock lock(hostedRequestMutex);
		if (requestedHostedModID != a_modID || requestedHostedPageID.empty())
			return std::nullopt;
		return viewLoad.RevisionFor(a_modID, requestedHostedPageID);
	}
	std::optional<std::uint64_t> BridgeController::ScriptViewRevision(std::uint64_t a_session, std::string_view a_modID)
	{
		if (session != a_session || ExternalOperationBlocked())
			return std::nullopt;
		const std::scoped_lock lock(hostedRequestMutex);
		if (requestedHostedModID != a_modID || requestedHostedPageID.empty())
			return std::nullopt;
		return viewLoad.RevisionFor(a_modID, requestedHostedPageID);
	}
	void BridgeController::CloseScriptView(std::uint64_t a_session, std::uint64_t a_revision, std::string_view a_modID, bool a_closeFrontend)
	{
		if (session != a_session || ExternalOperationBlocked())
			return;
		{
			const std::scoped_lock lock(hostedRequestMutex);
			if (requestedHostedModID != a_modID || requestedHostedPageID.empty() || !viewLoad.Remap(a_revision, {}))
				return;
			hostedPageRoute.Close(a_modID, requestedHostedPageID);
			requestedHostedModID.clear();
			requestedHostedPageID.clear();
		}
		// The drive waits for active writes and their commit confirmation. Never
		// dispatch a competing CloseConfig from a script's frontend request.
		QueueHostedDrive();
		if (a_closeFrontend) {
			QuickOpenWindow::Close();
			FrameworkApi::GetSingleton().SetOpen(false);
		}
		SKSE::log::info("Native MCM requested frontend close: mod={} close_framework={}", a_modID, a_closeFrontend);
	}
	bool BridgeController::RouteCommittedPage(const std::string& a_modID, const std::string& a_pageID, const HostedWriteNavigation& a_navigation)
	{
		if (!a_navigation)
			return false;
		const auto& [revision, target] = *a_navigation;
		const auto snapshot = snapshots.Get();
		const auto mod = std::ranges::find(snapshot->mods, a_modID, &MCMMod::stableID);
		if (mod == snapshot->mods.end())
			return false;
		const auto page = std::ranges::find_if(mod->pages, [&](const auto& a_page) { return a_page.index == target.index && a_page.rawName == target.name; });
		if (page == mod->pages.end() || page->stableID == a_pageID)
			return false;
		{
			const std::scoped_lock lock(hostedRequestMutex);
			if (requestedHostedModID != a_modID || requestedHostedPageID != a_pageID || !viewLoad.Remap(revision, page->stableID))
				return false;
			hostedPageRoute.Redirect(a_modID, a_pageID, page->stableID);
			requestedHostedPageID = page->stableID;
		}
		// DriveHostedPage builds this target normally; confirmation never marks an
		// unrelated page ready and never repeats the completed mutation callback.
		QueueHostedDrive();
		return true;
	}
	std::string BridgeController::ResolveHostedPage(std::string_view a_modID, std::string_view a_pageID)
	{
		const std::scoped_lock lock(hostedRequestMutex);
		return hostedPageRoute.Resolve(a_modID, a_pageID);
	}
	void BridgeController::ClearHostedPageRoute()
	{
		const std::scoped_lock lock(hostedRequestMutex);
		hostedPageRoute.Clear();
	}
	bool BridgeController::AdoptHostedPage(std::uint64_t& a_revision, const std::string& a_modID, std::string& a_pageID, const MCMPage& a_page)
	{
		if (a_pageID == a_page.stableID)
			return true;
		const auto snapshot = snapshots.Get();
		const auto mod = std::ranges::find(snapshot->mods, a_modID, &MCMMod::stableID);
		if (mod == snapshot->mods.end())
			return false;
		const auto page = std::ranges::find(mod->pages, a_page.stableID, &MCMPage::stableID);
		if (page == mod->pages.end() || page->rawName != a_page.rawName || page->index != a_page.index)
			return false;
		const std::scoped_lock lock(hostedRequestMutex);
		if (requestedHostedModID != a_modID || requestedHostedPageID != a_pageID || !viewLoad.Remap(a_revision, a_page.stableID))
			return false;
		const auto revision = viewLoad.RevisionFor(a_modID, a_page.stableID);
		if (!revision)
			return false;
		hostedPageRoute.Redirect(a_modID, a_pageID, a_page.stableID);
		requestedHostedPageID = a_page.stableID;
		a_pageID = a_page.stableID;
		a_revision = *revision;
		hostedPageKey = a_page.rawName;
		hostedPageIndex = a_page.index;
		return true;
	}
}
