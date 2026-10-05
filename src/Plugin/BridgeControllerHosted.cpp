#include "MCMBridge/Plugin/BridgeController.h"

#include "MCMBridge/Framework/FrameworkApi.h"
#include "MCMBridge/Plugin/GameSessionEvents.h"
#include "MCMBridge/Plugin/TaskScheduler.h"

#include <algorithm>

namespace
{
	const MCMBridge::MCMMod* FindMod(const MCMBridge::MCMSnapshot& a_snapshot, std::string_view a_modID)
	{
		const auto found = std::ranges::find(a_snapshot.mods, a_modID, &MCMBridge::MCMMod::stableID);
		return found != a_snapshot.mods.end() ? std::addressof(*found) : nullptr;
	}

	const MCMBridge::MCMPage* FindPage(const MCMBridge::MCMMod& a_mod, std::string_view a_pageID)
	{
		const auto found = std::ranges::find(a_mod.pages, a_pageID, &MCMBridge::MCMPage::stableID);
		return found != a_mod.pages.end() ? std::addressof(*found) : nullptr;
	}
}

namespace MCMBridge
{
	void BridgeController::DriveHostedPage()
	{
		if (!sessionReady.load() || !GameSessionEvents::CanUseGame())
			return;
		UpdateCaptureSession();
		if (ExternalOperationBlocked()) {
			return;
		}
		if (refreshing || activeScan || activeWrite || activeHelp || activeHostedPage || activeHostedClose) {
			return;
		}
		ResolveScriptPage();
		if (registryCheckPending.load()) {
			QueueRegistryCheck();
			return;
		}
		if (refreshRequested.exchange(false)) {
			RefreshOnGameThread(false);
			return;
		}

		std::string modID;
		std::string pageID;
		{
			const std::scoped_lock lock(hostedRequestMutex);
			modID = requestedHostedModID;
			pageID = requestedHostedPageID;
			if (navigationRecovery.blocked)
				return;
		}
		if (modID.empty() || pageID.empty()) {
			if (hostedScript) {
				CloseHostedSession();
			}
			return;
		}

		const auto  snapshot = snapshots.Get();
		const auto* mod = FindMod(*snapshot, modID);
		const auto* page = mod ? FindPage(*mod, pageID) : nullptr;
		const auto  live = std::ranges::find_if(liveEntries, [&](const auto& a_entry) {
			return a_entry.descriptor.stableID == modID;
		});
		if (!mod || !page || live == liveEntries.end() ||
			quarantined.contains(modID)) {
			if (hostedScript) {
				CloseHostedSession();
			}
			return;
		}

		if (hostedScript && hostedDescriptor.stableID != modID) {
			CloseHostedSession([this] { DriveHostedPage(); });
			return;
		}
		if (hostedScript && hostedReady && hostedPageID == pageID && viewLoad.Ready(modID, pageID) &&
			hostedScript->IsConfigOpen() && hostedScript->IsPageReady(page->index)) {
			ProcessWrites();
			return;
		}
		if (hostedScript && !hostedScript->IsConfigOpen()) {
			ClearHostedState();
		}
		if (!hostedScript) {
			hostedDescriptor = live->descriptor;
			hostedScript = live->adapter->CreateSession();
		}

		hostedReady = false;
		const auto operationSession = session;
		const auto revision = viewLoad.Request(modID, pageID);
		activeHostedPage = std::make_shared<HostedPageOperation>(
			hostedDescriptor,
			hostedScript,
			page->rawName,
			page->index,
			hostedScript->IsConfigOpen(),
			HostedPageMode::kActivate,
			HostedPageOperation::BusyCheck{},
			[this, operationSession, revision, modID, pageID](Result<MCMPage> a_result) mutable {
				FinishHostedPage(operationSession, revision, std::move(modID), std::move(pageID), std::move(a_result));
			},
			TaskScheduler::GetSingleton());
		activeHostedPage->SetMenuResolver(menuResolver);
		StartHostedPageOperation();
	}

	void BridgeController::StartHostedPageOperation()
	{
		const auto                        snapshot = snapshots.Get();
		const auto*                       mod = FindMod(*snapshot, hostedDescriptor.stableID);
		std::vector<ClassicPageSelection> pages;
		if (mod) {
			for (const auto& page : mod->pages) pages.push_back({ page.rawName, page.index });
		}
		const auto operation = activeHostedPage;
		operation->SetExpectedPages(std::move(pages));
		operation->Start();
	}

	void BridgeController::FinishHostedPage(
		std::uint64_t   a_session,
		std::uint64_t   a_revision,
		std::string     a_modID,
		std::string     a_pageID,
		Result<MCMPage> a_result)
	{
		if (a_session != session) {
			return;
		}
		const auto configOpen = activeHostedPage && activeHostedPage->IsConfigOpen();
		activeHostedPage.reset();
		if (!a_result || !configOpen) {
			const auto error = a_result ?
			                       BridgeError{ BridgeErrorCode::kUnavailable, "Hosted MCM config closed unexpectedly" } :
			                       a_result.error();
			SKSE::log::warn("Hosted page activation failed for {}: {}", a_modID, error.message);
			viewLoad.Complete(a_revision, error.message);
			if (error.code == BridgeErrorCode::kStaleSnapshot) {
				RecoverHostedNavigation(a_revision, a_modID, a_pageID);
				return;
			}
			const auto  snapshot = snapshots.Get();
			const auto* mod = FindMod(*snapshot, a_modID);
			const auto* page = mod ? FindPage(*mod, a_pageID) : nullptr;
			if (mod && page) {
				const auto rejected = writes.RemoveIf([&](const WriteCommand& a_command) {
					const auto& identity = a_command.expectedIdentity;
					return identity.ownerPlugin == mod->ownerPlugin && identity.questFormID == mod->questFormID &&
					       identity.scriptName == mod->scriptName && identity.pageKey == page->rawName && identity.pageIndex == page->index;
				});
				for (const auto& command : rejected) RejectWrite(command, error);
			}
			bool requestChanged{};
			{
				const std::scoped_lock lock(hostedRequestMutex);
				requestChanged = requestedHostedModID != a_modID || requestedHostedPageID != a_pageID;
			}
			const auto resume = requestChanged || refreshRequested.load();
			if (error.code == BridgeErrorCode::kTimedOut) {
				quarantined.insert(a_modID);
				ClearHostedState();
				ProcessWrites();
				if (resume)
					DriveHostedPage();
			} else if (configOpen) {
				CloseHostedSession([this, resume] {
					ProcessWrites();
					if (resume)
						DriveHostedPage();
				});
			} else {
				ClearHostedState();
				ProcessWrites();
				if (resume) {
					DriveHostedPage();
				}
			}
			return;
		}

		if (!viewLoad.Current(a_revision)) {
			hostedReady = false;
			DriveHostedPage();
			return;
		}
		{
			const std::scoped_lock lock(hostedRequestMutex);
			navigationRecovery = {};
		}
		if (!AdoptHostedPage(a_revision, a_modID, a_pageID, *a_result)) {
			viewLoad.Complete(a_revision, "The script-selected page is no longer available. Retry after navigation refresh.");
			hostedReady = false;
			return;
		}
		PublishHostedPage(a_modID, a_pageID, std::move(*a_result), true);
		viewLoad.Complete(a_revision);
		hostedPageID = std::move(a_pageID);
		hostedReady = true;
		ProcessWrites();
		DriveHostedPage();
	}

	void BridgeController::CloseHostedSession(std::function<void()> a_continuation)
	{
		if (a_continuation) {
			hostedCloseContinuations.push_back(std::move(a_continuation));
		}
		hostedReady = false;
		if (scriptContext.borrowed || activeHostedPage || activeHostedClose || activeHelp || (activeWrite && activeWriteHosted)) {
			return;
		}
		if (!hostedScript || !hostedScript->IsConfigOpen()) {
			ClearHostedState();
			RunHostedCloseContinuations();
			if (registryCheckPending.load()) {
				QueueRegistryCheck();
			}
			return;
		}

		const auto operationSession = session;
		activeHostedClose = std::make_shared<HostedCloseOperation>(
			hostedScript,
			[this, operationSession](Result<void> a_result) {
				if (operationSession != session) {
					return;
				}
				activeHostedClose.reset();
				if (!a_result) {
					SKSE::log::warn("Hosted MCM close failed: {}", a_result.error().message);
					if (a_result.error().code == BridgeErrorCode::kTimedOut) {
						quarantined.insert(hostedDescriptor.stableID);
					}
				}
				ClearHostedState();
				RunHostedCloseContinuations();
				if (registryCheckPending.load()) {
					QueueRegistryCheck();
				}
			},
			TaskScheduler::GetSingleton());
		const auto operation = activeHostedClose;
		operation->Start();
	}

	void BridgeController::ClearHostedState()
	{
		EndCaptureSession();
		scriptPageRequest.reset();
		viewLoad.Invalidate(true);
		++hostedRefreshToken;
		hostedScript.reset();
		hostedDescriptor = {};
		hostedPageID.clear();
		hostedPageKey.clear();
		hostedPageIndex = -1;
		hostedReady = false;
		activeWriteHosted = false;
	}

	void BridgeController::RunHostedCloseContinuations()
	{
		auto continuations = std::move(hostedCloseContinuations);
		hostedCloseContinuations.clear();
		for (auto& continuation : continuations) {
			if (continuation) {
				continuation();
			}
		}
	}

	void BridgeController::AbandonHostedSession(bool a_clearRequest)
	{
		if (activeHostedPage) {
			activeHostedPage->Cancel();
			activeHostedPage.reset();
		}
		if (activeHostedClose) {
			activeHostedClose->Cancel();
			activeHostedClose.reset();
		}
		if (activeHelp) {
			auto operation = std::move(activeHelp);
			operation->Cancel();
		}
		hostedCloseContinuations.clear();
		ClearHostedState();
		if (a_clearRequest) {
			const std::scoped_lock lock(hostedRequestMutex);
			navigationRecovery = {};
			requestedHostedModID.clear();
			requestedHostedPageID.clear();
			hostedPageRoute.Clear();
		}
		hostedDriveQueued.store(false);
		hostedRenderedThisFrame.store(false);
	}

	bool BridgeController::HasHostedSession() const
	{
		if (hostedScript || activeHostedPage || activeHostedClose || activeHelp) {
			return true;
		}
		const std::scoped_lock lock(hostedRequestMutex);
		return !requestedHostedModID.empty();
	}

	bool BridgeController::IsHostedTarget(const SettingIdentity& a_identity) const
	{
		return hostedReady && hostedScript && hostedScript->IsConfigOpen() &&
		       hostedScript->IsPageReady(hostedPageIndex) &&
		       hostedDescriptor.ownerPlugin == a_identity.ownerPlugin &&
		       hostedDescriptor.questFormID == a_identity.questFormID &&
		       hostedDescriptor.scriptName == a_identity.scriptName &&
		       hostedPageKey == a_identity.pageKey && hostedPageIndex == a_identity.pageIndex;
	}

	bool BridgeController::IsDesiredHostedTarget(const SettingIdentity& a_identity) const
	{
		std::string modID;
		std::string pageID;
		{
			const std::scoped_lock lock(hostedRequestMutex);
			modID = requestedHostedModID;
			pageID = requestedHostedPageID;
		}
		const auto  snapshot = snapshots.Get();
		const auto* mod = FindMod(*snapshot, modID);
		const auto* page = mod ? FindPage(*mod, pageID) : nullptr;
		return mod && page && mod->ownerPlugin == a_identity.ownerPlugin &&
		       mod->questFormID == a_identity.questFormID && mod->scriptName == a_identity.scriptName &&
		       page->rawName == a_identity.pageKey && page->index == a_identity.pageIndex;
	}
}
