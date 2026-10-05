#include "MCMBridge/Papyrus/ClassicWriteOperation.h"
#include "MCMBridge/Core/HostAudit.h"
#include "MCMBridge/Core/Slider.h"

#include <format>

namespace
{
	constexpr auto operationBudget = std::chrono::seconds(60);
}

namespace MCMBridge
{
	ClassicWriteOperation::ClassicWriteOperation(
		std::shared_ptr<IClassicScript> a_script,
		MCMControl                      a_control,
		WriteCommand                    a_command,
		BusyCheck                       a_busyCheck,
		Completion                      a_completion,
		IOperationTimer&                a_timer,
		ClassicWriteMode                a_mode,
		const IOperationClock&          a_clock) :
		script(std::move(a_script)),
		control(std::move(a_control)),
		command(std::move(a_command)),
		busyCheck(std::move(a_busyCheck)),
		completion(std::move(a_completion)),
		timer(a_timer),
		operation(operationBudget, a_clock, [this] { return script->MessageWaitDuration(); }),
		mode(a_mode)
	{}

	void ClassicWriteOperation::Start()
	{
		operation.Start();
		if (busyCheck && busyCheck()) {
			Finish(std::unexpected(BridgeError{ BridgeErrorCode::kBusy, "Journal MCM is active" }));
			return;
		}
		if (mode == ClassicWriteMode::kHosted) {
			configOpen = script->IsConfigOpen();
			if (!configOpen) {
				Finish(std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "Hosted MCM session is not open" }));
				return;
			}
			if (pagePrepared)
				ValidateAndApply();
			else
				SetPage(false);
		} else {
			Open();
		}
	}

	void ClassicWriteOperation::Cancel()
	{
		if (!finished) {
			const auto result = std::unexpected(BridgeError{ BridgeErrorCode::kStaleSnapshot, "Write was cancelled" });
			if (mode == ClassicWriteMode::kHosted) {
				Finish(result);
			} else {
				Close(result);
			}
		}
	}

	void ClassicWriteOperation::YieldToFrontend()
	{
		Finish(std::unexpected(BridgeError{ BridgeErrorCode::kBusy, "Journal MCM took ownership" }));
	}

	void ClassicWriteOperation::Open()
	{
		Dispatch({ .method = ClassicMethod::kOpenConfig }, [self = shared_from_this()] {
			self->configOpen = self->script->IsConfigOpen();
			if (!self->configOpen) {
				self->Finish(std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "OpenConfig did not initialize SkyUI buffers" }));
				return;
			}
			self->SetPage(false);
		});
	}

	void ClassicWriteOperation::SetPage(bool a_confirming)
	{
		if (a_confirming) {
			callbackResetPage = script->PageResetRevision() > resetBeforeCallback;
			// Navigation is not a value confirmation. Retain it while rebuilding the
			// original setting page; never repeat the mutation to restore navigation.
			if (auto redirect = script->TakePageRedirect())
				pageRedirect = std::move(redirect);
		}
		if (busyCheck && busyCheck()) {
			YieldToFrontend();
			return;
		}
		Dispatch(
			{ .method = ClassicMethod::kSetPage, .text = control.identity.pageKey, .integer = control.identity.pageIndex },
			[self = shared_from_this(), a_confirming] {
				if (a_confirming) {
					self->Confirm();
				} else {
					self->ValidateAndApply();
				}
			});
	}

	void ClassicWriteOperation::ValidateAndApply()
	{
		resetBeforeCallback = script->PageResetRevision();
		const auto activationControl = control.type == MCMControlType::kText || control.type == MCMControlType::kStepper;
		if (activationControl && command.intent == WriteIntent::kSetValue) {
			Close(std::unexpected(BridgeError{ BridgeErrorCode::kUnsupported, "Control requires an activation write" }));
			return;
		}
		if (!activationControl && command.intent == WriteIntent::kActivate) {
			Close(std::unexpected(BridgeError{ BridgeErrorCode::kUnsupported, "Control does not support activation writes" }));
			return;
		}
		if (!script->Matches(control.identity, control.type)) {
			Close(std::unexpected(BridgeError{ BridgeErrorCode::kStaleSnapshot, "Live control identity changed" }));
			return;
		}
		if (control.type == MCMControlType::kMenu) {
			RequestMenu(false);
			return;
		}
		if (control.type == MCMControlType::kColor) {
			RequestColor(false);
			return;
		}
		auto current = ReadCurrentValue();
		if (!current || *current != command.expectedValue) {
			Close(std::unexpected(BridgeError{ BridgeErrorCode::kStaleSnapshot, "Live control value changed" }));
			return;
		}
		if (command.intent == WriteIntent::kReset) {
			ApplyReset();
		} else if (activationControl) {
			ApplyText();
		} else if (control.type == MCMControlType::kToggle) {
			ApplyToggle(*current);
		} else if (control.type == MCMControlType::kSlider) {
			RequestSlider();
		} else if (control.type == MCMControlType::kKeymap) {
			ApplyKeymap();
		} else if (control.type == MCMControlType::kInput) {
			RequestInput();
		} else {
			Close(std::unexpected(BridgeError{ BridgeErrorCode::kUnsupported, "Control type is not writable" }));
		}
	}

	void ClassicWriteOperation::ApplyText()
	{
		Dispatch(
			{ .method = ClassicMethod::kSelectOption, .integer = control.identity.optionIndex },
			[self = shared_from_this()] { self->SetPage(true); });
	}

	void ClassicWriteOperation::ApplyToggle(const MCMValue& a_current)
	{
		const auto* currentValue = std::get_if<bool>(&a_current);
		const auto* desired = std::get_if<bool>(&command.desiredValue);
		if (!currentValue || !desired) {
			Close(std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Toggle command has an invalid value" }));
			return;
		}
		if (*currentValue == *desired) {
			Close(command.desiredValue);
			return;
		}
		Dispatch(
			{ .method = ClassicMethod::kSelectOption, .integer = control.identity.optionIndex },
			[self = shared_from_this()] { self->SetPage(true); });
	}

	void ClassicWriteOperation::ApplyKeymap()
	{
		const auto* desired = std::get_if<std::int32_t>(&command.desiredValue);
		if (!desired || *desired < -1 || *desired > 281) {
			Close(std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Keymap command has an invalid key code" }));
			return;
		}
		Dispatch(
			{ .method = ClassicMethod::kRemapKey,
				.text = command.conflictControl,
				.secondaryText = command.conflictName,
				.integer = control.identity.optionIndex,
				.secondaryInteger = *desired },
			[self = shared_from_this()] { self->SetPage(true); });
	}

	void ClassicWriteOperation::ApplyReset()
	{
		Dispatch(
			{ .method = ClassicMethod::kResetOption, .integer = control.identity.optionIndex },
			[self = shared_from_this()] { self->SetPage(true); });
	}

	void ClassicWriteOperation::Confirm()
	{
		if (!script->Matches(control.identity, control.type)) {
			// A completed callback may deliberately replace its own control. This
			// confirms execution, not the value of a control that no longer exists.
			Close(std::unexpected(callbackResetPage && script->IsPageReady(control.identity.pageIndex) ?
									  BridgeError{ BridgeErrorCode::kPageRebuilt, "Callback completed and rebuilt its page; target value is no longer observable" } :
									  BridgeError{ BridgeErrorCode::kStaleSnapshot, "Control identity changed after callback" }));
			return;
		}
		if (control.type == MCMControlType::kMenu) {
			RequestMenu(true);
			return;
		}
		if (control.type == MCMControlType::kColor) {
			RequestColor(true);
			return;
		}
		auto current = ReadCurrentValue();
		if (command.intent == WriteIntent::kActivate || command.intent == WriteIntent::kReset) {
			if (current) {
				Close(*current);
			} else {
				Close(std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Text callback did not rebuild its value" }));
			}
			return;
		}
		if (current && ControlValuesEqual(control, *current, command.desiredValue)) {
			Close(*current);
		} else {
			Close(std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData,
				std::format("Callback did not apply the requested value: requested={}, observed={}", AuditValue(command.desiredValue), current ? AuditValue(*current) : "unavailable") }));
		}
	}

	void ClassicWriteOperation::Close(Result<MCMValue> a_result)
	{
		if (!configOpen || mode == ClassicWriteMode::kHosted) {
			Finish(std::move(a_result));
			return;
		}
		closeResult = std::move(a_result);
		closing = true;
		Dispatch({ .method = ClassicMethod::kCloseConfig }, [self = shared_from_this()] {
			self->configOpen = false;
			self->closing = false;
			auto result = std::move(*self->closeResult);
			self->closeResult.reset();
			self->Finish(std::move(result));
		});
	}

	void ClassicWriteOperation::Finish(Result<MCMValue> a_result)
	{
		if (finished) {
			return;
		}
		finished = true;
		deadline.Cancel();
		operation.Invalidate();
		if (!a_result && a_result.error().code == BridgeErrorCode::kTimedOut)
			script->RetireExecution();
		if (menuCaptureActive) {
			menuResolver->CancelCapture();
			menuCaptureActive = false;
		}
		if (completion) {
			completion(std::move(a_result));
		}
	}

}
