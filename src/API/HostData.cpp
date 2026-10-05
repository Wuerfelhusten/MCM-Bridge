#include "MCMBridge/API/HostData.h"

#include <limits>
#include <stdexcept>

namespace
{
	std::uint32_t Count(std::size_t a_count)
	{
		if (a_count > std::numeric_limits<std::uint32_t>::max())
			throw std::length_error("Native host view exceeds the C count range");
		return static_cast<std::uint32_t>(a_count);
	}
}

namespace MCMBridge
{
	std::uint32_t HostControlType(MCMControlType a_type)
	{
		switch (a_type) {
		case MCMControlType::kText:
		case MCMControlType::kStepper:
			return MCM_HOST_TEXT;
		case MCMControlType::kToggle:
			return MCM_HOST_TOGGLE;
		case MCMControlType::kSlider:
			return MCM_HOST_SLIDER;
		case MCMControlType::kMenu:
			return MCM_HOST_MENU;
		case MCMControlType::kColor:
			return MCM_HOST_COLOR;
		case MCMControlType::kKeymap:
			return MCM_HOST_KEYMAP;
		case MCMControlType::kInput:
			return MCM_HOST_INPUT;
		default:
			return 0;
		}
	}

	MCMHostArgument HostValue(const MCMValue& a_value)
	{
		if (const auto* toggle = std::get_if<bool>(&a_value))
			return { MCM_HOST_INTEGER, *toggle ? 1 : 0, 0, nullptr };
		if (const auto* integer = std::get_if<std::int32_t>(&a_value))
			return { MCM_HOST_INTEGER, *integer, 0, nullptr };
		if (const auto* color = std::get_if<std::uint32_t>(&a_value))
			return { MCM_HOST_INTEGER, static_cast<std::int32_t>(*color), 0, nullptr };
		if (const auto* number = std::get_if<float>(&a_value))
			return { MCM_HOST_FLOAT, 0, *number, nullptr };
		if (const auto* text = std::get_if<std::string>(&a_value))
			return { MCM_HOST_STRING, 0, 0, text->c_str() };
		return {};
	}

	const char* HostModName(std::string_view a_identity, const std::string& a_fallback)
	{
		const auto separator = a_identity.find("::");
		return separator == std::string_view::npos ? a_fallback.c_str() : a_identity.data() + separator + 2;
	}

	HostData::HostData(HostDataInput a_input) : input(std::move(a_input))
	{
		mods.reserve(input.registry.size());
		if (input.active) {
			const auto size = input.active->pages.size();
			pages.reserve(size);
			controls.resize(size);
			menus.resize(size);
			for (std::size_t i = 0; i < size; ++i) {
				const auto& page = input.active->pages[i];
				auto&       pageControls = controls[i];
				menus[i].resize(page.controls.size());
				pageControls.reserve(page.controls.size());
				for (std::size_t j = 0; j < page.controls.size(); ++j) {
					const auto&        control = page.controls[j];
					MCMHostControlView view{};
					view.option_index = control.identity.optionIndex;
					view.type = HostControlType(control.type);
					view.disabled = control.disabled;
					view.hidden = control.hidden;
					view.label = control.rawLabel.c_str();
					view.state_name = control.identity.stateName.c_str();
					view.setting_id = control.identity.explicitID.c_str();
					view.value = HostValue(control.value);
					view.menu_index = view.menu_default = -1;
					if (control.slider && control.slider->availability == MetadataAvailability::kAvailable) {
						const auto& slider = *control.slider;
						view.dialog_ready = 1;
						view.dialog_numbers[0] = slider.start;
						view.dialog_numbers[1] = slider.defaultValue;
						view.dialog_numbers[2] = slider.minimum;
						view.dialog_numbers[3] = slider.maximum;
						view.dialog_numbers[4] = slider.step;
					} else if (control.menu && control.menu->availability == MetadataAvailability::kAvailable) {
						view.dialog_ready = 1;
						view.menu_index = control.menu->selectedIndex;
						view.menu_default = control.menu->defaultIndex;
						for (const auto& item : control.menu->options)
							menus[i][j].push_back(item.c_str());
						view.menu_count = Count(menus[i][j].size());
						view.menu_items = menus[i][j].data();
					} else if (control.color && control.color->availability == MetadataAvailability::kAvailable) {
						view.dialog_ready = 1;
						view.dialog_numbers[0] = static_cast<float>(control.color->start);
						view.dialog_numbers[1] = static_cast<float>(control.color->defaultValue);
					} else if (control.input && control.input->availability == MetadataAvailability::kAvailable) {
						view.dialog_ready = 1;
						view.dialog_text = control.input->startText.c_str();
					}
					pageControls.push_back(view);
				}
				pages.push_back({ page.rawName.c_str(), page.index, input.currentPage == page.index ? 1U : 0U, Count(pageControls.size()), pageControls.data() });
			}
		}
		for (const auto& mod : input.registry) {
			const bool active = input.active && input.active->interopID == mod.interopID;
			mods.push_back({ mod.interopID.c_str(), HostModName(mod.interopID, mod.displayName), mod.scriptName.c_str(), mod.ownerPlugin.c_str(), mod.questFormID, mod.pageScopedState ? 1U : 0U, active ? 1U : 0U, active ? Count(pages.size()) : 0U, active ? pages.data() : nullptr });
		}
		static_cast<void>(Count(mods.size()));
	}

	MCMHostDataView HostData::View() const
	{
		return { input.session, Count(mods.size()), mods.data() };
	}
}
