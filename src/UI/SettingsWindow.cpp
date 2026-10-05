#include "MCMBridge/UI/SettingsWindow.h"
#include "MCMBridge/Plugin/BridgeSettingsService.h"
#include "MCMBridge/Plugin/WritePauseService.h"
#include "SKSEMenuFramework.h"

namespace MCMBridge::SettingsWindow
{
	void __stdcall Render()
	{
		auto& preferences = BridgeSettingsService::GetSingleton();
		auto  settings = preferences.Get();
		ImGuiMCP::TextWrapped("Mod Configuration always opens in Menu Framework.");
		if (ImGuiMCP::Checkbox("Close Journal when redirecting", &settings.closeJournalOnRedirect)) {
			preferences.SetCloseJournalOnRedirect(settings.closeJournalOnRedirect);
		}
		ImGuiMCP::TextWrapped("Keeping the Journal open retains its game pause.");
		ImGuiMCP::Separator();
		auto& service = WritePauseService::GetSingleton();
		auto  enabled = service.Enabled();
		if (ImGuiMCP::Checkbox("Pause game during setting changes", &enabled)) {
			service.SetEnabled(enabled);
		}
		ImGuiMCP::TextWrapped(
			"Enabled by default. Uses Skyrim's menu pause while setting callbacks and the immediate "
			"page commit are running. The pause remains until all queued changes have finished.");
		ImGuiMCP::TextWrapped(
			"Menu Framework stays interactive. Timeouts display a message and release the affected "
			"change. Pauses owned by other menus are never released by MCM Bridge.");
		const auto pending = std::format("Pending setting changes: {}", service.Pending());
		ImGuiMCP::TextUnformatted(pending.c_str());
		ImGuiMCP::TextDisabled("Saved in Data/SKSE/plugins/MCMBridge.ini");
	}
}
