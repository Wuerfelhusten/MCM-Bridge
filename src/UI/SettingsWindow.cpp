#include "MCMBridge/UI/SettingsWindow.h"
#include "MCMBridge/Framework/FrameworkApi.h"
#include "MCMBridge/Plugin/BridgeSettingsService.h"
#include "MCMBridge/Plugin/WritePauseService.h"
#include "MCMBridge/UI/FrontendUI.h"

namespace MCMBridge::SettingsWindow
{
	void __stdcall Render()
	{
		auto& preferences = BridgeSettingsService::GetSingleton();
		auto  settings = preferences.Get();
		BridgeUI::TextWrapped("Mod Configuration always opens in the selected MCM frontend.");
		auto& frontend = FrameworkApi::GetSingleton();
		BridgeUI::BeginDisabled(!frontend.HasFlick() || !frontend.HasMenuFramework());
		if (BridgeUI::Checkbox("Prefer FLICK", &settings.preferFlick))
			preferences.SetPreferFlick(settings.preferFlick);
		BridgeUI::EndDisabled();
		BridgeUI::TextWrapped("Uses the available frontend when only one is installed. A switch waits for callbacks, backup/restore and dialogs to finish.");
		BridgeUI::Text("Active frontend: %s", frontend.Active() == Frontend::kFlick ? "FLICK" : "SKSE Menu Framework");
		if (BridgeUI::Checkbox("Close Journal when redirecting", &settings.closeJournalOnRedirect)) {
			preferences.SetCloseJournalOnRedirect(settings.closeJournalOnRedirect);
		}
		BridgeUI::TextWrapped("Keeping the Journal open retains its game pause.");
		BridgeUI::Separator();
		auto& service = WritePauseService::GetSingleton();
		auto  enabled = service.Enabled();
		if (BridgeUI::Checkbox("Pause game during setting changes", &enabled)) {
			service.SetEnabled(enabled);
		}
		BridgeUI::TextWrapped(
			"Enabled by default. Uses Skyrim's menu pause while setting callbacks and the immediate "
			"page commit are running. The pause remains until all queued changes have finished.");
		BridgeUI::TextWrapped(
			"The frontend stays interactive. Timeouts display a message and release the affected "
			"change. Pauses owned by other menus are never released by MCM Bridge.");
		const auto pending = std::format("Pending setting changes: {}", service.Pending());
		BridgeUI::TextUnformatted(pending.c_str());
		BridgeUI::TextDisabled("Saved in Data/SKSE/plugins/MCMBridge.ini");
	}
}
