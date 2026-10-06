#include "MCMBridge/UI/FlickSettingsWindow.h"
#include "MCMBridge/Plugin/BridgeSettingsService.h"
#include "MCMBridge/UI/FrontendUI.h"

namespace MCMBridge::FlickSettingsWindow
{
	void Render()
	{
		auto&                preferences = BridgeSettingsService::GetSingleton();
		auto                 settings = preferences.Get();
		constexpr std::array names{ "Top panel", "Left panel", "Dropdown" };
		BridgeUI::TextWrapped("Choose how MCM pages are selected in FLICK. SKSE Menu Framework is not affected.");
		bool changed{};
		BridgeUI::TextUnformatted("Page selection");
		BridgeUI::SetNextItemWidth(BridgeUI::GetContentRegionAvail().x);
		auto selector = static_cast<int>(settings.flickPageSelector);
		if (BridgeUI::Combo("##flick-page-selector", &selector, names.data(), static_cast<int>(names.size()))) {
			settings.flickPageSelector = static_cast<FlickPageSelector>(selector);
			changed = true;
		}
		if (settings.flickPageSelector == FlickPageSelector::kTop) {
			BridgeUI::TextUnformatted("Visible page rows (additional pages scroll)");
			BridgeUI::SetNextItemWidth(BridgeUI::GetContentRegionAvail().x);
			changed |= BridgeUI::SliderInt("##flick-page-rows", &settings.flickPageRows, 1, 6);
		} else if (settings.flickPageSelector == FlickPageSelector::kLeft) {
			BridgeUI::TextUnformatted("Page panel width");
			BridgeUI::SetNextItemWidth(BridgeUI::GetContentRegionAvail().x);
			changed |= BridgeUI::SliderInt("##flick-page-width", &settings.flickPageWidth, 15, 45, "%d%%");
		}
		if (changed)
			preferences.SetFlickPages(settings.flickPageSelector, settings.flickPageRows, settings.flickPageWidth);
		BridgeUI::TextDisabled("Saved in Data/SKSE/plugins/MCMBridge.ini");
	}
}
