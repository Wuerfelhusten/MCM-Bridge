#include "MCMBridge/UI/BrowserWindow.h"

#include "MCMBridge/Core/MCMOrganization.h"
#include "MCMBridge/Plugin/BridgeController.h"
#include "MCMBridge/Plugin/BridgeSettingsService.h"
#include "MCMBridge/UI/MCMRangeSettings.h"
#include "SKSEMenuFramework.h"

#include <array>

namespace
{
	std::array<char, 256> search{};
	std::array<char, 512> alias{};
	std::string           selected;

	void LoadAlias(const MCMBridge::BridgeSettings& a_settings)
	{
		alias.fill('\0');
		const auto found = a_settings.aliases.find(selected);
		if (found != a_settings.aliases.end())
			std::copy_n(found->second.data(), std::min(found->second.size(), alias.size() - 1), alias.data());
	}
}

namespace MCMBridge
{
	void __stdcall BrowserWindow::Render()
	{
		auto&      service = BridgeSettingsService::GetSingleton();
		const auto settings = service.Get();
		const auto snapshot = BridgeController::GetSingleton().Snapshot();
		ImGuiMCP::TextWrapped("Manage display names only. Original MCM names and MCMMemory profiles are unchanged.");
		ImGuiMCP::InputText("Search names", search.data(), search.size());
		std::vector<const MCMMod*>                   ordered;
		std::unordered_map<std::string, std::string> names;
		for (const auto& mod : snapshot->mods) {
			names.emplace(mod.stableID, ResolveMCMAlias(settings, mod.stableID, mod.displayName));
			ordered.push_back(&mod);
		}
		std::ranges::sort(ordered, [&](const auto* a_left, const auto* a_right) {
			const auto left = MCMNameSortKey(names.at(a_left->stableID));
			const auto right = MCMNameSortKey(names.at(a_right->stableID));
			return left != right ? left < right : a_left->stableID < a_right->stableID;
		});
		const auto query = MCMNameSortKey(search.data());
		if (ImGuiMCP::BeginChild("MCM name list", { 0, ImGuiMCP::GetTextLineHeightWithSpacing() * 12 }, ImGuiMCP::ImGuiChildFlags_Border)) {
			for (const auto* mod : ordered) {
				const auto& name = names.at(mod->stableID);
				if (!MCMNameSortKey(name).contains(query) && !MCMNameSortKey(mod->displayName).contains(query))
					continue;
				const auto label = std::format("{}##{}", name, mod->stableID);
				if (ImGuiMCP::Selectable(label.c_str(), selected == mod->stableID)) {
					selected = mod->stableID;
					LoadAlias(settings);
				}
			}
		}
		ImGuiMCP::EndChild();
		const auto mod = std::ranges::find(snapshot->mods, selected, &MCMMod::stableID);
		if (mod != snapshot->mods.end()) {
			ImGuiMCP::TextWrapped("Original: %s", mod->displayName.c_str());
			ImGuiMCP::TextWrapped("Displayed: %s", names.at(selected).c_str());
			ImGuiMCP::InputText("Display name", alias.data(), alias.size());
			if (ImGuiMCP::Button("Save name"))
				service.SetAlias(selected, alias.data());
			ImGuiMCP::SameLine();
			if (ImGuiMCP::Button("Reset name")) {
				alias.fill('\0');
				service.SetAlias(selected, {});
			}
			ImGuiMCP::TextDisabled("An empty name restores the original. Names are sorted automatically.");
		} else
			ImGuiMCP::TextDisabled("Select an MCM to edit its display name.");
		MCMRangeSettings::Render();
	}
}
