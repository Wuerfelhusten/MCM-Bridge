#include "MCMBridge/Localization/SnapshotLocalizer.h"

#include "SKSE/Translation.h"

namespace
{
	void Translate(std::string& a_value)
	{
		if (!a_value.starts_with('$')) {
			return;
		}

		std::string translated;
		if (SKSE::Translation::Translate(a_value, translated) && !translated.empty()) {
			a_value = std::move(translated);
		}
	}

}

namespace MCMBridge::SnapshotLocalizer
{
	std::string LocalizeText(std::string a_text)
	{
		Translate(a_text);
		return a_text;
	}

	void Localize(MCMMod& a_mod)
	{
		Translate(a_mod.displayName);
		for (auto& page : a_mod.pages) {
			Translate(page.displayName);
			Translate(page.title);
			for (auto& control : page.controls) {
				if (control.rawLabel.empty())
					control.rawLabel = control.label;
				Translate(control.label);
				Translate(control.help);
				if (control.type == MCMControlType::kText) {
					if (const auto* rawValue = std::get_if<std::string>(&control.value)) {
						control.displayValue = LocalizeText(*rawValue);
					}
				} else {
					Translate(control.displayValue);
				}
				if (control.slider) {
					Translate(control.slider->format);
				}
				if (control.menu) {
					control.menu->displayOptions = control.menu->options;
					control.menu->displayShortNames = control.menu->shortNames;
					for (auto& option : control.menu->displayOptions) {
						Translate(option);
					}
					for (auto& shortName : control.menu->displayShortNames) {
						Translate(shortName);
					}
				}
			}
		}
	}
}
