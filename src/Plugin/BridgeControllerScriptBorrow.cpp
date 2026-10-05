#include "MCMBridge/Plugin/BridgeController.h"

#include "MCMBridge/Framework/FrameworkApi.h"
#include "MCMBridge/Localization/SnapshotLocalizer.h"
#include "MCMBridge/Papyrus/NativeFacade.h"
#include "MCMBridge/Papyrus/NativeHostUI.h"
#include "MCMBridge/Plugin/TaskScheduler.h"
#include "MCMBridge/Plugin/WritePauseService.h"

namespace MCMBridge
{
	bool BridgeController::BorrowScriptContext(const RE::BSTSmartPointer<RE::BSScript::Object>& a_script, const std::string& a_mod, std::uint32_t a_stack)
	{
		if (directContext.id || refreshing || activeScan || activeWrite || activeHelp || activeHostedPage || activeHostedClose ||
			!hostedScript || !hostedReady || hostedDescriptor.stableID != a_mod || !hostedScript->IsConfigOpen())
			return false;
		const auto revision = CaptureScriptView(a_mod);
		const auto ownerID = reinterpret_cast<std::uintptr_t>(a_script.get());
		const auto token = NativeFacadeSession().TokenForOwner(ownerID);
		if (!revision || !token)
			return false;
		const auto    owner = std::format("Papyrus:{}", a_stack);
		std::uint64_t lease{};
		{
			const std::scoped_lock lock(externalOperationMutex);
			// Borrow only an idle frontend. Never displace a waiting native client.
			if (externalOperation.BlocksNormalWork() || WritePauseService::GetSingleton().Pending())
				return false;
			if (externalOperation.Begin(owner, false, false, lease) != ExternalAdmission::kReady)
				return false;
			if (!scriptContext.calls.Adopt(session, ownerID, token)) {
				externalOperation.End(lease);
				return false;
			}
		}
		scriptContext.script = a_script;
		scriptContext.owner = owner;
		scriptContext.mod = a_mod;
		scriptContext.lease = lease;
		scriptContext.borrowed = true;
		scriptContext.viewRevision = *revision;
		viewLoad.Suspend(*revision);
		hostedReady = false;
		++hostedRefreshToken;
		return true;
	}

	void BridgeController::ReturnScriptContext()
	{
		const auto lease = scriptContext.lease;
		const auto revision = scriptContext.viewRevision;
		const auto modID = scriptContext.mod;
		const auto pageID = hostedPageID;
		TaskScheduler::GetSingleton().Cancel(scriptContext.timer);
		scriptContext.executionDeadline.reset();
		WritePauseService::GetSingleton().Complete(std::exchange(scriptContext.pause, 0));
		NativeHostUI::DetachScriptStack(scriptContext.stack.get());
		// The frontend still owns the native binding. Release admission, not config.
		scriptContext.calls.Reset();
		scriptContext.script.reset();
		scriptContext.stack.reset();
		scriptContext.lease = scriptContext.timer = 0;
		scriptContext.borrowed = false;
		scriptContext.admitted.clear();

		const auto current = hostedScript ? hostedScript->ReadCurrentPage() : std::nullopt;
		const auto names = hostedScript ? hostedScript->ReadNavigationPages() : std::nullopt;
		if (names) {
			auto       snapshot = *snapshots.Get();
			const auto mod = std::ranges::find(snapshot.mods, modID, &MCMMod::stableID);
			if (mod != snapshot.mods.end()) {
				bool changed{};
				if (current && current->index == -1 && current->name.empty()) {
					// A completed opening page is real content, even beside named pages.
					MergeScriptNavigation(*mod, *names, "", changed, true);
				} else {
					changed = MergeNavigation(*mod, *names);
				}
				if (changed) {
					SnapshotLocalizer::Localize(*mod);
					snapshots.Publish(std::move(snapshot));
					FrameworkApi::GetSingleton().SynchronizeMCMs(snapshots.Get()->mods);
				}
			}
		}
		if (current && names && viewLoad.Current(revision) && hostedScript->IsConfigOpen()) {
			const auto operationSession = session;
			activeHostedPage = std::make_shared<HostedPageOperation>(
				hostedDescriptor, hostedScript, current->name, current->index, true, HostedPageMode::kReadCurrent,
				HostedPageOperation::BusyCheck{},
				[this, operationSession, revision, modID, pageID](Result<MCMPage> a_result) {
					FinishHostedPage(operationSession, revision, modID, pageID, std::move(a_result));
				},
				TaskScheduler::GetSingleton());
			activeHostedPage->SetMenuResolver(menuResolver);
			// Install the operation before releasing the lease, so queued UI work
			// cannot rebuild this already completed page between admission and read.
			StartHostedPageOperation();
		} else if (viewLoad.Current(revision)) {
			const std::scoped_lock lock(hostedRequestMutex);
			navigationRecovery.blocked = true;
			viewLoad.Complete(revision, "External MCM call did not leave a readable page. Retry to reload it.");
		}
		EndExternalOperation(lease);
	}
}
