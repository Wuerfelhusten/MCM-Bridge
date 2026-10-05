#include "MCMBridge/Plugin/BridgeController.h"

#include "MCMBridge/Plugin/TaskScheduler.h"
#include "MCMBridge/UI/OriginalPageNavigator.h"

namespace
{
	constexpr auto          navigationRetryDelay = std::chrono::milliseconds(100);
	constexpr std::uint32_t maximumNavigationAttempts = 50;
}

namespace MCMBridge
{
	void BridgeController::OpenOriginal(std::string a_modID, std::string a_pageID)
	{
		if (auto* tasks = SKSE::GetTaskInterface()) {
			tasks->AddTask([modID = std::move(a_modID), pageID = std::move(a_pageID)]() mutable {
				auto& controller = GetSingleton();
				controller.OpenOriginalOnGameThread(
					std::move(modID), std::move(pageID), controller.session, 0);
			});
		}
	}

	void BridgeController::OpenOriginalOnGameThread(
		std::string   a_modID,
		std::string   a_pageID,
		std::uint64_t a_session,
		std::uint32_t a_attempt)
	{
		if (IsNativeHost() || a_session != session) {
			return;
		}
		ScheduleHostedRequest({}, {});
		if (HasHostedSession()) {
			CloseHostedSession(
				[this, modID = std::move(a_modID), pageID = std::move(a_pageID), a_session, a_attempt]() mutable {
					OpenOriginalOnGameThread(std::move(modID), std::move(pageID), a_session, a_attempt);
				});
			return;
		}
		if (ExternalOperationBlocked() || refreshing || activeScan || activeWrite || activeHelp || activeHostedPage || activeHostedClose) {
			if (a_attempt >= maximumNavigationAttempts) {
				SKSE::log::error("Could not open the original MCM while a bridge operation was active");
				return;
			}
			TaskScheduler::GetSingleton().After(navigationRetryDelay,
				[modID = std::move(a_modID), pageID = std::move(a_pageID), a_session, a_attempt]() mutable {
					GetSingleton().OpenOriginalOnGameThread(std::move(modID), std::move(pageID), a_session, a_attempt + 1);
				});
			return;
		}

		const auto snapshot = snapshots.Get();
		const auto mod = std::ranges::find(snapshot->mods, a_modID, &MCMMod::stableID);
		if (quarantined.contains(a_modID)) {
			SKSE::log::error("Could not open a timed-out MCM in the current game session");
			return;
		}
		const MCMPage* page{};
		if (mod != snapshot->mods.end()) {
			const auto foundPage = std::ranges::find(mod->pages, a_pageID, &MCMPage::stableID);
			if (foundPage != mod->pages.end()) {
				page = std::addressof(*foundPage);
			}
		}
		if (!page || !page->customContent) {
			SKSE::log::error("Could not resolve the requested original MCM page");
			return;
		}

		const auto live = std::ranges::find_if(liveEntries, [&](const auto& a_entry) {
			return a_entry.descriptor.stableID == a_modID;
		});
		if (live == liveEntries.end() || (live->configIndex < 0 && live->registryID.empty())) {
			SKSE::log::error("Could not resolve the SkyUI config index for the original MCM page");
			return;
		}
		OriginalPageNavigator::GetSingleton().Open({ live->configIndex, page->rawName, page->index, live->registryID });
	}
}
