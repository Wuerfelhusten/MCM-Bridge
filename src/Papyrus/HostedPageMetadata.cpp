#include "MCMBridge/Papyrus/HostedPageOperation.h"

namespace MCMBridge
{
	void HostedPageOperation::ReadNextMetadata()
	{
		if (mode == HostedPageMode::kReadCurrent && script->ReadCurrentPage() != ClassicPageSelection{ pageKey, pageIndex }) {
			Fail({ BridgeErrorCode::kStaleSnapshot, "External MCM page changed during metadata collection" });
			return;
		}
		if (!ValidatePages())
			return;
		if (!script->IsPageReady(pageIndex)) {
			Fail({ BridgeErrorCode::kStaleSnapshot, "MCM page changed during metadata collection" });
			return;
		}
		while (metadataIndex < page->controls.size()) {
			const auto index = metadataIndex++;
			auto&      control = page->controls[index];
			if (control.hidden)
				continue;
			const auto    option = control.identity.optionIndex;
			ClassicMethod method;
			switch (control.type) {
			case MCMControlType::kSlider:
				method = ClassicMethod::kRequestSliderDialogData;
				break;
			case MCMControlType::kMenu:
				method = ClassicMethod::kRequestMenuDialogData;
				menuResolver->BeginCapture(control.identity);
				break;
			case MCMControlType::kColor:
				method = ClassicMethod::kRequestColorDialogData;
				break;
			case MCMControlType::kInput:
				method = ClassicMethod::kRequestInputDialogData;
				break;
			default:
				continue;
			}
			Dispatch({ .method = method, .integer = option }, [self = shared_from_this(), index, option] {
				auto& target = self->page->controls[index];
				switch (target.type) {
				case MCMControlType::kSlider:
					if (auto metadata = self->script->ReadSliderMetadata(option)) {
						metadata->format = target.slider ? target.slider->format : std::string{};
						target.defaultValue = metadata->defaultValue;
						target.value = metadata->start;
						target.slider = std::move(*metadata);
					} else if (!target.disabled)
						target.writeCapability = WriteCapability::kReadOnly;
					break;
				case MCMControlType::kMenu:
					{
						auto metadata = self->script->ReadMenuMetadata(option);
						auto options = self->menuResolver->Resolve(target.identity);
						if (metadata && options) {
							options->selectedIndex = metadata->selectedIndex;
							options->defaultIndex = metadata->defaultIndex;
							target.value = options->selectedIndex;
							target.defaultValue = options->defaultIndex;
							target.menu = std::move(*options);
							target.writeCapability = target.disabled ? WriteCapability::kDisabled : WriteCapability::kWritable;
						} else {
							self->menuResolver->CancelCapture();
							if (!target.disabled)
								target.writeCapability = WriteCapability::kMissingOptions;
						}
						break;
					}
				case MCMControlType::kColor:
					if (auto metadata = self->script->ReadColorMetadata(option)) {
						target.value = metadata->start;
						target.defaultValue = metadata->defaultValue;
						target.color = std::move(*metadata);
					} else if (!target.disabled)
						target.writeCapability = WriteCapability::kReadOnly;
					break;
				case MCMControlType::kInput:
					if (auto metadata = self->script->ReadInputMetadata(option))
						target.input = std::move(*metadata);
					break;
				default:
					break;
				}
				self->ReadNextMetadata();
			});
			return;
		}
		Finish(std::move(*page));
	}
}
