#include "MCMBridge/Plugin/BridgeController.h"

#include "MCMBridge/API/HostEvents.h"
#include "MCMBridge/Localization/SnapshotLocalizer.h"
#include "MCMBridge/Plugin/GameSessionEvents.h"
#include "MCMBridge/Plugin/TaskScheduler.h"
#include "MCMBridge/Plugin/WritePauseService.h"
#include "MCMBridge/UI/WriteNotifications.h"
#include "MCMBridge/Write/KeymapConflictResolver.h"
#include "MCMBridge/Write/WriteTiming.h"
#include "MCMBridge/Write/WriteValidator.h"

namespace
{
	MCMBridge::WriteTimingOutcome TimingOutcome(MCMBridge::BridgeErrorCode a_code)
	{
		switch (a_code) {
		case MCMBridge::BridgeErrorCode::kPageRebuilt:
			return MCMBridge::WriteTimingOutcome::kPageRebuilt;
		case MCMBridge::BridgeErrorCode::kTimedOut:
			return MCMBridge::WriteTimingOutcome::kTimedOut;
		case MCMBridge::BridgeErrorCode::kStaleSnapshot:
			return MCMBridge::WriteTimingOutcome::kStaleSnapshot;
		default:
			return MCMBridge::WriteTimingOutcome::kRejected;
		}
	}

	MCMBridge::WriteStatus ToWriteStatus(MCMBridge::BridgeErrorCode a_code)
	{
		switch (a_code) {
		case MCMBridge::BridgeErrorCode::kTimedOut:
			return MCMBridge::WriteStatus::kTimedOut;
		case MCMBridge::BridgeErrorCode::kStaleSnapshot:
			return MCMBridge::WriteStatus::kStaleSnapshot;
		default:
			return MCMBridge::WriteStatus::kRejected;
		}
	}
}

namespace MCMBridge
{
	void BridgeController::ProcessWrites()
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
		if (registryCheckPending.load()) {
			QueueRegistryCheck();
			return;
		}
		if (refreshRequested.load()) {
			DriveHostedPage();
			return;
		}
		auto command = writes.TryPop();
		if (!command) {
			return;
		}
		if (IsClassicMCMActive()) {
			if (!waitingForMCM)
				SKSE::log::info("Deferring setting changes while a classic MCM or registry operation is active");
			waitingForMCM = true;
			DeferWrite(std::move(*command), std::chrono::milliseconds(100));
			return;
		}
		if (waitingForMCM) {
			waitingForMCM = false;
			SKSE::log::info("Resuming queued setting changes after the classic MCM became idle");
		}
		const auto snapshot = snapshots.Get();
		auto       validated = ValidateWrite(*snapshot, *command);
		if (!validated) {
			RejectWrite(*command, validated.error());
			ProcessWrites();
			return;
		}

		auto control = validated->get();
		if (control.type == MCMControlType::kKeymap && command->intent == WriteIntent::kSetValue) {
			if (const auto* keyCode = std::get_if<std::int32_t>(&command->desiredValue)) {
				auto [conflictControl, conflictName] = ResolveKeymapConflict(*snapshot, control, *keyCode);
				command->conflictControl = std::move(conflictControl);
				command->conflictName = std::move(conflictName);
			}
		}
		auto live = FindLive(control.identity);
		if (!live || quarantined.contains(live->descriptor.stableID)) {
			RejectWrite(*command, { BridgeErrorCode::kUnavailable, "Live MCM is unavailable" });
			ProcessWrites();
			return;
		}

		const auto hosted = IsHostedTarget(control.identity);
		if (!hosted && IsDesiredHostedTarget(control.identity)) {
			writes.PushFront(std::move(*command));
			DriveHostedPage();
			return;
		}
		if (!hosted && hostedScript) {
			writes.PushFront(std::move(*command));
			CloseHostedSession([this] { ProcessWrites(); });
			return;
		}

		const auto pause = WritePauseService::GetSingleton().Begin(command->pauseTicket);
		if (!pause) {
			RejectWrite(*command, pause.error());
			ProcessWrites();
			return;
		}
		if (!*pause) {
			DeferWrite(std::move(*command), std::chrono::milliseconds(10));
			return;
		}

		activeWriteTiming = command->timing;
		if (activeWriteTiming) {
			activeWriteTiming->BeginApply(hosted, control.label);
		}
		snapshots.SetWriteStatus(command->settingID, WriteStatus::kPending);
		command->recordingSnapshot = snapshots.Get();
		command->recordingID = ++nextRecordingID;
		command->recordingSession = captureSessionID;
		activeRecordingID = command->recordingID;
		recordingAccepted = recordingDeclined = false;
		const auto operationCommand = *command;
		const auto operationSession = session;
		const auto viewRevision = hosted ? viewLoad.RevisionFor(live->descriptor.stableID, hostedPageID) : std::nullopt;
		auto       script = hosted ? hostedScript : live->adapter->CreateSession();
		activeWriteHosted = hosted;
		activeWrite = std::make_shared<ClassicWriteOperation>(
			std::move(script),
			std::move(control),
			operationCommand,
			[this] { return IsClassicMCMActive(); },
			[this, operationCommand, operationSession, hosted, viewRevision, modID = live->descriptor.stableID](Result<MCMValue> a_result) {
				if (operationSession != session) {
					return;
				}
				HostedWriteNavigation navigation;
				if (activeWrite && viewRevision) {
					if (auto redirect = activeWrite->PageRedirect())
						navigation.emplace(*viewRevision, std::move(*redirect));
				}
				activeWrite.reset();
				activeWriteHosted = false;
				if (hosted && a_result) {
					std::optional<std::string> displayValue;
					if (const auto* rawText = std::get_if<std::string>(std::addressof(*a_result))) {
						displayValue = SnapshotLocalizer::LocalizeText(*rawText);
					}
					snapshots.SetWriteStatus(
						operationCommand.settingID,
						WriteStatus::kPending,
						*a_result,
						std::move(displayValue));
					StartHostedCommit(operationCommand, *a_result, std::move(navigation));
					return;
				}
				if (!a_result && a_result.error().code == BridgeErrorCode::kTimedOut) {
					quarantined.insert(modID);
					if (hosted)
						ClearHostedState();
				} else if (hosted && !a_result && a_result.error().code == BridgeErrorCode::kBusy && originalMCMOpen.load()) {
					ClearHostedState();
				} else if (hosted && !a_result) {
					CloseHostedSession([this, operationCommand, a_result, modID] {
						if (quarantined.contains(modID)) {
							FinishWrite(operationCommand, std::unexpected(BridgeError{ BridgeErrorCode::kTimedOut, "CloseConfig timed out while cleaning up the setting change" }), true);
						} else {
							FinishWrite(operationCommand, a_result, true);
						}
						ProcessWrites();
						DriveHostedPage();
					});
					return;
				}
				FinishWrite(operationCommand, a_result, hosted);
				ProcessWrites();
				DriveHostedPage();
			},
			TaskScheduler::GetSingleton(),
			hosted ? ClassicWriteMode::kHosted : ClassicWriteMode::kStandalone);
		const auto operation = activeWrite;
		HostEvents::GetSingleton().Write(MCM_HOST_USER_CHANGING, operationCommand.recordingSession, operationCommand, operationCommand.expectedValue);
		operation->Start();
	}

	void BridgeController::FinishWrite(
		const WriteCommand&     a_command,
		const Result<MCMValue>& a_result,
		bool                    a_hosted)
	{
		if (a_command.timing) {
			a_command.timing->Finish(
				a_result ? WriteTimingOutcome::kApplied : TimingOutcome(a_result.error().code),
				a_result ? std::string_view{} : std::string_view(a_result.error().message));
		}
		if (activeWriteTiming == a_command.timing) {
			activeWriteTiming.reset();
		}
		WritePauseService::GetSingleton().Complete(a_command.pauseTicket);
		if (a_result) {
			SKSE::log::info("Applied write for {}", a_command.settingID);
			std::optional<std::string> displayValue;
			if (const auto* rawText = std::get_if<std::string>(std::addressof(*a_result))) {
				displayValue = SnapshotLocalizer::LocalizeText(*rawText);
			}
			snapshots.SetWriteStatus(
				a_command.settingID,
				WriteStatus::kApplied,
				*a_result,
				std::move(displayValue));
			HostEvents::GetSingleton().Write(MCM_HOST_USER_CHANGE, a_command.recordingSession, a_command, *a_result, recordingAccepted, recordingDeclined);
			if (!a_hosted) {
				RequestRefresh();
			}
		} else {
			const bool rebuilt = a_result.error().code == BridgeErrorCode::kPageRebuilt;
			if (rebuilt)
				SKSE::log::info("MCM callback completed for {}: {}; no confirmed value recorded", a_command.settingID, a_result.error().message);
			else
				SKSE::log::warn("Write failed for {}: {}", a_command.settingID, a_result.error().message);
			HostEvents::GetSingleton().Write(MCM_HOST_USER_REJECTED, a_command.recordingSession, a_command, {}, recordingAccepted, recordingDeclined);
			snapshots.SetWriteStatus(a_command.settingID, rebuilt ? WriteStatus::kIdle : ToWriteStatus(a_result.error().code));
			NotifyWriteTimeout(a_command, a_result.error());
		}
		activeRecordingID = 0;
	}

	void BridgeController::ObserveUserConfirmation(std::uint64_t a_edit, bool a_accepted)
	{
		if (!a_edit || a_edit != activeRecordingID)
			return;
		recordingAccepted |= a_accepted;
		recordingDeclined |= !a_accepted;
	}

	void BridgeController::RejectWrite(const WriteCommand& a_command, const BridgeError& a_error)
	{
		if (a_command.timing)
			a_command.timing->Finish(TimingOutcome(a_error.code), a_error.message);
		SKSE::log::warn("Rejected write for {}: {}", a_command.settingID, a_error.message);
		snapshots.SetWriteStatus(a_command.settingID, ToWriteStatus(a_error.code));
		WritePauseService::GetSingleton().Complete(a_command.pauseTicket);
		NotifyWriteTimeout(a_command, a_error);
	}

	void BridgeController::NotifyWriteTimeout(const WriteCommand& a_command, const BridgeError& a_error)
	{
		if (a_error.code != BridgeErrorCode::kTimedOut)
			return;
		std::string name = a_command.expectedIdentity.scriptName;
		const auto  snapshot = snapshots.Get();
		for (const auto& mod : snapshot->mods) {
			for (const auto& page : mod.pages) {
				for (const auto& control : page.controls) {
					if (control.identity.stableID == a_command.settingID) {
						name = std::format("{} / {} / {}", mod.displayName, page.displayName, control.label);
					}
				}
			}
		}
		WriteNotifications::Show(std::format(
			"Setting change timed out: {}\n\n{}\n\n"
			"This request no longer holds the game pause. Other pending changes still count. "
			"A callback that already started may finish later; the change is not confirmed. "
			"See MCMBridge.log for details.",
			name, a_error.message));
	}
}
