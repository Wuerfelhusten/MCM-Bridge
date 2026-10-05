#include "MCMBridge/Framework/FrameworkApi.h"
#include "MCMBridge/Plugin/BridgeController.h"

namespace MCMBridge
{
	void BridgeController::ResumeGameUI()
	{
		if (auto* tasks = SKSE::GetTaskInterface())
			tasks->AddTask([] {
				auto& controller = GetSingleton();
				controller.DriveHostedPage();
				controller.ProcessWrites();
				if (controller.hostedReady)
					controller.ScheduleHostedAutoRefresh();
			});
	}

	void BridgeController::ObserveCustomContent(const MCMMod& a_mod, const MCMPage& a_page, CustomContentOrigin a_origin)
	{
		if (!IsNativeHost() || !a_page.customContent)
			return;
		{
			const std::scoped_lock lock(hostedRequestMutex);
			if (a_origin == CustomContentOrigin::kUser &&
				(requestedHostedModID != a_mod.stableID || requestedHostedPageID != a_page.stableID))
				return;
			if (!customVisits.Observe(a_mod.stableID, a_page.stableID, a_origin))
				return;
		}
		if (a_origin != CustomContentOrigin::kUser) {
			SKSE::log::debug("Native custom content result: origin={} mod={} owner={} page=\"{}\" source=\"{}\" placeholder=\"{}\"",
				a_origin == CustomContentOrigin::kScan ? "scan" : "restore",
				a_mod.stableID, a_mod.ownerPlugin, a_page.rawName, a_page.customContent->source, CustomContentPlaceholder(a_page.customContent->source));
			return;
		}
		SKSE::log::info("Native custom content opened: origin=user mod={} owner={} page=\"{}\" source=\"{}\" placeholder=\"{}\"",
			a_mod.stableID, a_mod.ownerPlugin, a_page.rawName, a_page.customContent->source, CustomContentPlaceholder(a_page.customContent->source));
	}

	void BridgeController::OpenFrameworkView()
	{
		++navigationEpoch;
		frameworkViewOpen.store(true);
		nextNavigationPoll.store(0);
		viewLoad.Invalidate();
		RequestRefresh(true);
	}

	void BridgeController::CloseFrameworkView()
	{
		ClearHostedPageRoute();
		frameworkViewOpen.store(false);
		++navigationEpoch;
		CloseHostedView();
	}

	bool BridgeController::IsHostedViewReady(std::string_view a_modID, std::string_view a_pageID) const
	{
		return viewLoad.Ready(a_modID, a_pageID);
	}

	std::string BridgeController::HostedViewError() const
	{
		return viewLoad.Error();
	}

	void BridgeController::BeginFrameworkFrame()
	{
		QueueNavigationPoll();
		if (!sessionReady.load())
			RequestRefresh(true);
		hostedRenderedThisFrame.store(false);
	}

	void BridgeController::ObserveHostedPage(std::string_view a_modID, std::string_view a_pageID)
	{
		hostedRenderedThisFrame.store(true);
		ScheduleHostedRequest(std::string(a_modID), std::string(a_pageID));
	}

	void BridgeController::EndFrameworkFrame()
	{
		if (!hostedRenderedThisFrame.load()) {
			CloseHostedView();
		}
	}

	void BridgeController::CloseHostedView()
	{
		ScheduleHostedRequest({}, {});
	}

	void BridgeController::ScheduleHostedRequest(std::string a_modID, std::string a_pageID)
	{
		bool changed{};
		{
			const std::scoped_lock lock(hostedRequestMutex);
			viewLoad.Request(a_modID, a_pageID);
			changed = requestedHostedModID != a_modID || requestedHostedPageID != a_pageID;
			if (changed) {
				navigationRecovery = {};
				customVisits.Leave();
			}
			requestedHostedModID = std::move(a_modID);
			requestedHostedPageID = std::move(a_pageID);
		}
		if (changed) {
			QueueHostedDrive();
		}
	}

	void BridgeController::QueueHostedDrive()
	{
		if (hostedDriveQueued.exchange(true)) {
			return;
		}
		if (auto* tasks = SKSE::GetTaskInterface()) {
			tasks->AddTask([] {
				auto& controller = GetSingleton();
				controller.hostedDriveQueued.store(false);
				controller.DriveHostedPage();
			});
		} else {
			hostedDriveQueued.store(false);
		}
	}

}
