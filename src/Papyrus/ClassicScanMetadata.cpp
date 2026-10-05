#include "MCMBridge/Papyrus/ClassicScanOperation.h"

namespace MCMBridge
{
	void ClassicScanOperation::ReadNextMetadata()
	{
		auto& controls = mod.pages.back().controls;
		while (metadataIndex < controls.size()) {
			const auto index = metadataIndex++;
			if (metadataFilter && !metadataFilter(controls[index])) {
				controls[index].metadataCurrent = false;
				continue;
			}
			if (controls[index].type == MCMControlType::kSlider) {
				ReadSlider(index);
				return;
			}
			if (controls[index].type == MCMControlType::kMenu) {
				ReadMenu(index);
				return;
			}
			if (controls[index].type == MCMControlType::kColor) {
				ReadColor(index);
				return;
			}
			if (controls[index].type == MCMControlType::kInput) {
				ReadInput(index);
				return;
			}
		}
		AdvancePage();
	}

	void ClassicScanOperation::ReadSlider(std::size_t a_controlIndex)
	{
		if (busyCheck && busyCheck()) {
			YieldToFrontend();
			return;
		}
		const auto optionIndex = mod.pages.back().controls[a_controlIndex].identity.optionIndex;
		Dispatch(
			{ .method = ClassicMethod::kRequestSliderDialogData, .integer = optionIndex },
			[self = shared_from_this(), a_controlIndex, optionIndex] {
				auto& control = self->mod.pages.back().controls[a_controlIndex];
				auto  metadata = self->script->ReadSliderMetadata(optionIndex);
				if (metadata) {
					metadata->format = control.slider ? control.slider->format : std::string{};
					// The dialog cursor is not the setting's page value.
					control.defaultValue = metadata->defaultValue;
					control.slider = std::move(*metadata);
				} else if (!control.disabled) {
					control.writeCapability = WriteCapability::kReadOnly;
				}
				self->ReadNextMetadata();
			});
	}

	void ClassicScanOperation::ReadMenu(std::size_t a_controlIndex)
	{
		if (busyCheck && busyCheck()) {
			YieldToFrontend();
			return;
		}
		const auto optionIndex = mod.pages.back().controls[a_controlIndex].identity.optionIndex;
		menuResolver.BeginCapture(mod.pages.back().controls[a_controlIndex].identity);
		Dispatch(
			{ .method = ClassicMethod::kRequestMenuDialogData, .integer = optionIndex },
			[self = shared_from_this(), a_controlIndex, optionIndex] {
				auto& control = self->mod.pages.back().controls[a_controlIndex];
				auto  dialog = self->script->ReadMenuMetadata(optionIndex);
				if (dialog) {
					control.menu = std::move(*dialog);
					control.value = control.menu->selectedIndex;
					control.defaultValue = control.menu->defaultIndex;
				}
				if (auto options = self->menuResolver.Resolve(control.identity); options) {
					if (control.menu) {
						options->selectedIndex = control.menu->selectedIndex;
						options->defaultIndex = control.menu->defaultIndex;
					}
					control.menu = std::move(*options);
					control.writeCapability = control.disabled ? WriteCapability::kDisabled : WriteCapability::kWritable;
				} else {
					self->menuResolver.CancelCapture();
					control.writeCapability = control.disabled ? WriteCapability::kDisabled : WriteCapability::kMissingOptions;
				}
				self->ReadNextMetadata();
			});
	}

	void ClassicScanOperation::ReadColor(std::size_t a_controlIndex)
	{
		if (busyCheck && busyCheck()) {
			YieldToFrontend();
			return;
		}
		const auto optionIndex = mod.pages.back().controls[a_controlIndex].identity.optionIndex;
		Dispatch(
			{ .method = ClassicMethod::kRequestColorDialogData, .integer = optionIndex },
			[self = shared_from_this(), a_controlIndex, optionIndex] {
				auto& control = self->mod.pages.back().controls[a_controlIndex];
				auto  metadata = self->script->ReadColorMetadata(optionIndex);
				if (metadata) {
					control.value = metadata->start;
					control.defaultValue = metadata->defaultValue;
					control.color = std::move(*metadata);
				} else if (!control.disabled) {
					control.writeCapability = WriteCapability::kReadOnly;
				}
				self->ReadNextMetadata();
			});
	}

	void ClassicScanOperation::ReadInput(std::size_t a_controlIndex)
	{
		if (busyCheck && busyCheck()) {
			YieldToFrontend();
			return;
		}
		const auto optionIndex = mod.pages.back().controls[a_controlIndex].identity.optionIndex;
		Dispatch(
			{ .method = ClassicMethod::kRequestInputDialogData, .integer = optionIndex },
			[self = shared_from_this(), a_controlIndex, optionIndex] {
				auto& control = self->mod.pages.back().controls[a_controlIndex];
				auto  metadata = self->script->ReadInputMetadata(optionIndex);
				if (metadata) {
					control.input = std::move(*metadata);
				}
				self->ReadNextMetadata();
			});
	}

}
