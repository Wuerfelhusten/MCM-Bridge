#include "MCMBridge/Plugin/BridgeController.h"

#include "MCMBridge/Core/HostedPageMerge.h"
#include "MCMBridge/Localization/SnapshotLocalizer.h"
#include "MCMBridge/Plugin/GameSessionEvents.h"
#include "MCMBridge/Plugin/TaskScheduler.h"
#include "MCMBridge/Write/WriteTiming.h"
#include "MCMBridge/Write/WriteValidator.h"

#include <algorithm>
#include <array>

namespace
{
	constexpr std::array hostedRefreshDelays{
		std::chrono::milliseconds(50),
		std::chrono::milliseconds(100),
		std::chrono::milliseconds(200),
		std::chrono::milliseconds(300),
		std::chrono::milliseconds(500)
	};
	constexpr auto hostedRefreshRetryDelay = std::chrono::milliseconds(50);

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
	void BridgeController::NotifyFrontendInvalidation()
	{
		if (frontendInvalidationQueued.exchange(true)) {
			return;
		}
		const auto operationSession = session;
		TaskScheduler::GetSingleton().After(std::chrono::milliseconds(20), [operationSession] {
			auto& controller = GetSingleton();
			controller.frontendInvalidationQueued.store(false);
			if (controller.session != operationSession) {
				return;
			}
			if (controller.hostedReady) {
				controller.ScheduleHostedAutoRefresh();
			} else {
				controller.QueueHostedDrive();
			}
		});
	}

	void BridgeController::PublishHostedPage(std::string_view a_modID, std::string_view a_pageID, MCMPage a_page, bool a_freshMetadata)
	{
		const auto previous = snapshots.Get();
		if (const auto* mod = FindMod(*previous, a_modID)) {
			if (const auto* oldPage = FindPage(*mod, a_pageID)) {
				if (!oldPage->controls.empty() && a_page.controls.empty()) {
					SKSE::log::warn("MCM hosted page became empty: mod={} page={}", a_modID, a_pageID);
				}
				if (!a_freshMetadata)
					MergeHostedPageState(*oldPage, a_page);
			}
		}
		MCMMod pageMod{
			.stableID = hostedDescriptor.stableID,
			.displayName = hostedDescriptor.displayName,
			.backend = hostedDescriptor.backend,
			.ownerPlugin = hostedDescriptor.ownerPlugin,
			.questFormID = hostedDescriptor.questFormID,
			.scriptName = hostedDescriptor.scriptName,
			.interopID = hostedDescriptor.interopID,
			.pageScopedState = hostedDescriptor.pageScopedState,
			.pages = { std::move(a_page) }
		};
		if (pageMod.backend == MCMBackendKind::kMCMHelper)
			pageMod = MergeHelper(std::move(pageMod));
		SnapshotLocalizer::Localize(pageMod);
		hostedPageKey = pageMod.pages.front().rawName;
		hostedPageIndex = pageMod.pages.front().index;
		const auto published = snapshots.ReplacePage(a_modID, std::move(pageMod.pages.front()));
		if (published != previous) {
			SKSE::log::info("MCM hosted page published: mod={} page={} generation={}", a_modID, a_pageID, published->generation);
		} else {
			SKSE::log::debug("MCM hosted page unchanged: mod={} page={} generation={}", a_modID, a_pageID, previous->generation);
		}
	}

	void BridgeController::ScheduleHostedAutoRefresh()
	{
		if (!hostedReady || !hostedScript || !hostedScript->IsConfigOpen()) {
			return;
		}
		const auto token = ++hostedRefreshToken;
		const auto operationSession = session;
		TaskScheduler::GetSingleton().After(hostedRefreshDelays.front(), [operationSession, token] {
			auto& controller = GetSingleton();
			if (controller.session == operationSession) {
				controller.RunHostedAutoRefresh(token, 0);
			}
		});
	}

	void BridgeController::RunHostedAutoRefresh(std::uint64_t a_token, std::size_t a_pass)
	{
		if (!GameSessionEvents::CanUseGame())
			return;
		if (a_token != hostedRefreshToken || a_pass >= hostedRefreshDelays.size()) {
			return;
		}
		if (ExternalOperationBlocked()) {
			return;
		}
		if (refreshing || activeScan || activeWrite || activeHelp || activeHostedPage || activeHostedClose || IsClassicMCMActive()) {
			const auto operationSession = session;
			TaskScheduler::GetSingleton().After(hostedRefreshRetryDelay, [operationSession, a_token, a_pass] {
				auto& controller = GetSingleton();
				if (controller.session == operationSession) {
					controller.RunHostedAutoRefresh(a_token, a_pass);
				}
			});
			return;
		}
		if (!hostedReady || !hostedScript || !hostedScript->IsConfigOpen()) {
			return;
		}
		{
			const std::scoped_lock lock(hostedRequestMutex);
			if (requestedHostedModID != hostedDescriptor.stableID || requestedHostedPageID != hostedPageID) {
				return;
			}
		}

		const auto operationSession = session;
		activeHostedPage = std::make_shared<HostedPageOperation>(
			hostedDescriptor,
			hostedScript,
			hostedPageKey,
			hostedPageIndex,
			true,
			HostedPageMode::kActivate,
			[this] { return IsClassicMCMActive(); },
			[this, operationSession, a_token, a_pass](Result<MCMPage> a_result) mutable {
				FinishHostedAutoRefresh(operationSession, a_token, a_pass, std::move(a_result));
			},
			TaskScheduler::GetSingleton());
		StartHostedPageOperation();
	}

	void BridgeController::StartHostedCommit(WriteCommand a_command, MCMValue a_confirmedValue, HostedWriteNavigation a_navigation)
	{
		if (a_command.timing) {
			a_command.timing->BeginCommit();
		}
		if (!hostedReady || !hostedScript || !hostedScript->IsConfigOpen()) {
			FinishWrite(
				a_command,
				std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "Hosted MCM config is unavailable for commit" }),
				true);
			ProcessWrites();
			DriveHostedPage();
			return;
		}
		++hostedRefreshToken;
		hostedReady = false;
		const auto operationSession = session;
		const auto modID = hostedDescriptor.stableID;
		const auto pageID = hostedPageID;
		activeHostedPage = std::make_shared<HostedPageOperation>(
			hostedDescriptor,
			hostedScript,
			hostedPageKey,
			hostedPageIndex,
			true,
			HostedPageMode::kCommit,
			[this] { return IsClassicMCMActive(); },
			[this, operationSession, modID, pageID, command = std::move(a_command), confirmedValue = std::move(a_confirmedValue), navigation = std::move(a_navigation)](
				Result<MCMPage> a_result) mutable {
				FinishHostedCommit(
					operationSession,
					std::move(modID),
					std::move(pageID),
					std::move(command),
					std::move(confirmedValue),
					std::move(a_result), std::move(navigation));
			},
			TaskScheduler::GetSingleton());
		StartHostedPageOperation();
	}

	void BridgeController::FinishHostedCommit(
		std::uint64_t         a_session,
		std::string           a_modID,
		std::string           a_pageID,
		WriteCommand          a_command,
		MCMValue              a_confirmedValue,
		Result<MCMPage>       a_result,
		HostedWriteNavigation a_navigation)
	{
		if (a_session != session) {
			return;
		}
		const auto configOpen = activeHostedPage && activeHostedPage->IsConfigOpen();
		activeHostedPage.reset();
		if (!a_result || !configOpen) {
			const auto error = a_result ?
			                       BridgeError{ BridgeErrorCode::kUnavailable, "Hosted MCM config closed during write commit" } :
			                       a_result.error();
			SKSE::log::warn("Hosted write commit failed for {}: {}", a_command.settingID, error.message);
			if (error.code == BridgeErrorCode::kStaleSnapshot) {
				FinishWrite(a_command, std::unexpected(error), true);
				if (const auto revision = viewLoad.RevisionFor(a_modID, a_pageID))
					RecoverHostedNavigation(*revision, a_modID, a_pageID);
				else
					QueueHostedDrive();
				return;
			}
			if (error.code == BridgeErrorCode::kBusy && originalMCMOpen.load()) {
				ClearHostedState();
			} else if (error.code == BridgeErrorCode::kTimedOut) {
				quarantined.insert(hostedDescriptor.stableID);
				ClearHostedState();
			} else if (configOpen) {
				CloseHostedSession([this, command = std::move(a_command), error, a_modID] {
					const auto failure = quarantined.contains(a_modID) ?
					                         BridgeError{ BridgeErrorCode::kTimedOut, "CloseConfig timed out while cleaning up the setting change" } :
					                         error;
					FinishWrite(command, std::unexpected(failure), true);
					ProcessWrites();
					DriveHostedPage();
				});
				return;
			} else {
				ClearHostedState();
			}
			FinishWrite(a_command, std::unexpected(error), true);
			ProcessWrites();
			DriveHostedPage();
			return;
		}

		PublishHostedPage(a_modID, a_pageID, std::move(*a_result), true);
		hostedPageID = std::move(a_pageID);
		hostedReady = true;
		const auto confirmed = ConfirmWrite(*snapshots.Get(), a_command, a_confirmedValue);
		FinishWrite(a_command, confirmed, true);
		if (!confirmed || !RouteCommittedPage(a_modID, hostedPageID, a_navigation))
			ScheduleHostedAutoRefresh();
		ProcessWrites();
		DriveHostedPage();
	}

	void BridgeController::FinishHostedAutoRefresh(
		std::uint64_t   a_session,
		std::uint64_t   a_token,
		std::size_t     a_pass,
		Result<MCMPage> a_result)
	{
		if (a_session != session) {
			return;
		}
		const auto configOpen = activeHostedPage && activeHostedPage->IsConfigOpen();
		activeHostedPage.reset();
		if (a_token != hostedRefreshToken) {
			ProcessWrites();
			DriveHostedPage();
			return;
		}
		if (!a_result || !configOpen) {
			const auto error = a_result ?
			                       BridgeError{ BridgeErrorCode::kUnavailable, "Hosted MCM config closed during automatic refresh" } :
			                       a_result.error();
			SKSE::log::warn("Automatic hosted page refresh failed for {}: {}", hostedPageID, error.message);
			if (error.code == BridgeErrorCode::kStaleSnapshot) {
				if (const auto revision = viewLoad.RevisionFor(hostedDescriptor.stableID, hostedPageID))
					RecoverHostedNavigation(*revision, hostedDescriptor.stableID, hostedPageID);
				else
					QueueHostedDrive();
				return;
			}
			if (error.code == BridgeErrorCode::kBusy && originalMCMOpen.load()) {
				ClearHostedState();
			} else if (error.code == BridgeErrorCode::kTimedOut) {
				quarantined.insert(hostedDescriptor.stableID);
				ClearHostedState();
			} else if (!configOpen) {
				ClearHostedState();
			}
			ProcessWrites();
			DriveHostedPage();
			return;
		}

		PublishHostedPage(hostedDescriptor.stableID, hostedPageID, std::move(*a_result));
		const auto nextPass = a_pass + 1;
		if (nextPass < hostedRefreshDelays.size()) {
			const auto operationSession = session;
			TaskScheduler::GetSingleton().After(hostedRefreshDelays[nextPass], [operationSession, a_token, nextPass] {
				auto& controller = GetSingleton();
				if (controller.session == operationSession) {
					controller.RunHostedAutoRefresh(a_token, nextPass);
				}
			});
		}
		ProcessWrites();
		DriveHostedPage();
	}
}
