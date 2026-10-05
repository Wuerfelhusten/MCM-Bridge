#include "MCMBridge/Papyrus/ClassicWriteOperation.h"

#include "MCMBridge/Core/Slider.h"

#include <algorithm>
#include <bit>

namespace
{
	std::optional<float> ToFloat(const MCMBridge::MCMValue& a_value)
	{
		if (const auto* number = std::get_if<float>(&a_value)) {
			return *number;
		}
		if (const auto* integer = std::get_if<std::int32_t>(&a_value)) {
			return static_cast<float>(*integer);
		}
		return std::nullopt;
	}
}

namespace MCMBridge
{
	std::optional<MCMValue> ClassicWriteOperation::ReadCurrentValue() const
	{
		if (control.type != MCMControlType::kStepper) {
			return script->ReadValue(control.type, control.identity.optionIndex);
		}
		const auto  live = script->ReadValue(MCMControlType::kText, control.identity.optionIndex);
		const auto* displayed = live ? std::get_if<std::string>(std::addressof(*live)) : nullptr;
		if (!displayed || !control.menu) {
			return std::nullopt;
		}
		const auto findIndex = [&](const auto& a_options) -> std::optional<MCMValue> {
			const auto found = std::ranges::find(a_options, *displayed);
			if (found == a_options.end()) {
				return std::nullopt;
			}
			return MCMValue{ static_cast<std::int32_t>(std::distance(a_options.begin(), found)) };
		};
		if (auto index = findIndex(control.menu->options)) {
			return index;
		}
		return findIndex(control.menu->shortNames);
	}

	void ClassicWriteOperation::RequestSlider()
	{
		DispatchDialog(
			{ .method = ClassicMethod::kRequestSliderDialogData, .integer = control.identity.optionIndex },
			[self = shared_from_this()] {
				auto metadata = self->script->ReadSliderMetadata(self->control.identity.optionIndex);
				auto requested = ToFloat(self->command.desiredValue);
				if (!metadata || !requested) {
					self->Close(std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Slider command or metadata is invalid" }));
					return;
				}
				auto normalized = NormalizeSliderValue(*requested, metadata->minimum, metadata->maximum, metadata->step);
				if (!normalized) {
					self->Close(std::unexpected(normalized.error()));
					return;
				}
				self->command.desiredValue = *normalized;
				self->control.slider = *metadata;
				self->Dispatch(
					{ .method = ClassicMethod::kSetSliderValue, .number = *normalized },
					[self] { self->SetPage(true); });
			});
	}

	void ClassicWriteOperation::RequestMenu(bool a_confirming)
	{
		if (!a_confirming && menuResolver) {
			menuResolver->BeginCapture(control.identity);
			menuCaptureActive = true;
		}
		DispatchDialog(
			{ .method = ClassicMethod::kRequestMenuDialogData, .integer = control.identity.optionIndex },
			[self = shared_from_this(), a_confirming] {
				auto metadata = self->script->ReadMenuMetadata(self->control.identity.optionIndex);
				if (self->menuCaptureActive) {
					auto options = self->menuResolver->Resolve(self->control.identity);
					self->menuResolver->CancelCapture();
					self->menuCaptureActive = false;
					if (!options) {
						self->Close(std::unexpected(BridgeError{ BridgeErrorCode::kUnsupported, "Menu options could not be recaptured" }));
						return;
					}
					if (metadata)
						metadata->options = std::move(options->options);
				}
				if (!metadata) {
					self->Close(std::unexpected(metadata.error()));
					return;
				}
				if (!a_confirming && self->control.menu && !metadata->options.empty() && metadata->options != self->control.menu->options) {
					self->Close(std::unexpected(BridgeError{ BridgeErrorCode::kStaleSnapshot, "Menu options changed" }));
					return;
				}
				const MCMValue current = metadata->selectedIndex;
				if (a_confirming) {
					if (self->command.intent == WriteIntent::kReset || current == self->command.desiredValue) {
						self->Close(current);
					} else {
						self->Close(std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Menu callback did not apply the requested index" }));
					}
					return;
				}
				if (current != self->command.expectedValue) {
					self->Close(std::unexpected(BridgeError{ BridgeErrorCode::kStaleSnapshot, "Live menu index changed" }));
					return;
				}
				if (self->command.intent == WriteIntent::kReset) {
					self->ApplyReset();
					return;
				}
				const auto* desired = std::get_if<std::int32_t>(&self->command.desiredValue);
				if (!desired || !self->control.menu || *desired < 0 || static_cast<std::size_t>(*desired) >= self->control.menu->options.size()) {
					self->Close(std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Menu index is outside the captured option list" }));
					return;
				}
				self->Dispatch(
					{ .method = ClassicMethod::kSetMenuIndex, .integer = *desired },
					[self] { self->SetPage(true); });
			});
	}

	void ClassicWriteOperation::RequestColor(bool a_confirming)
	{
		DispatchDialog(
			{ .method = ClassicMethod::kRequestColorDialogData, .integer = control.identity.optionIndex },
			[self = shared_from_this(), a_confirming] {
				auto metadata = self->script->ReadColorMetadata(self->control.identity.optionIndex);
				if (!metadata) {
					self->Close(std::unexpected(metadata.error()));
					return;
				}
				const MCMValue current = metadata->start;
				if (a_confirming) {
					if (self->command.intent == WriteIntent::kReset || current == self->command.desiredValue) {
						self->Close(current);
					} else {
						self->Close(std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Color callback did not apply the requested value" }));
					}
					return;
				}
				if (current != self->command.expectedValue) {
					self->Close(std::unexpected(BridgeError{ BridgeErrorCode::kStaleSnapshot, "Live color changed" }));
					return;
				}
				if (self->command.intent == WriteIntent::kReset) {
					self->ApplyReset();
					return;
				}
				const auto* desired = std::get_if<std::uint32_t>(&self->command.desiredValue);
				if (!desired) {
					self->Close(std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Color command has an invalid value" }));
					return;
				}
				self->Dispatch(
					{ .method = ClassicMethod::kSetColorValue, .integer = std::bit_cast<std::int32_t>(*desired) },
					[self] { self->SetPage(true); });
			});
	}

	void ClassicWriteOperation::RequestInput()
	{
		const auto* desired = std::get_if<std::string>(&command.desiredValue);
		if (!desired) {
			Close(std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Input command has an invalid value" }));
			return;
		}
		DispatchDialog(
			{ .method = ClassicMethod::kRequestInputDialogData, .integer = control.identity.optionIndex },
			[self = shared_from_this(), value = *desired] {
				self->Dispatch(
					{ .method = ClassicMethod::kSetInputText, .text = value },
					[self] { self->SetPage(true); });
			});
	}
}
