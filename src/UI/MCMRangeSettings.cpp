#include "MCMBridge/UI/MCMRangeSettings.h"

#include "MCMBridge/Core/MCMOrganization.h"
#include "MCMBridge/Plugin/BridgeController.h"
#include "MCMBridge/Plugin/BridgeSettingsService.h"
#include "SKSEMenuFramework.h"

namespace MCMBridge::MCMRangeSettings
{
	void Render()
	{
		auto& service = BridgeSettingsService::GetSingleton();
		auto  settings = service.Get();
		ImGuiMCP::Separator();
		if (ImGuiMCP::Checkbox("Group MCMs in an MCMs folder", &settings.groupMCMs))
			service.SetGroupMCMs(settings.groupMCMs);
		ImGuiMCP::BeginDisabled(!settings.groupMCMs);
		if (ImGuiMCP::Checkbox("Alphabetical subfolders", &settings.alphabeticMCMs))
			service.SetMCMRanges(settings.alphabeticMCMs, settings.mcmRangeEnds);
		ImGuiMCP::EndDisabled();
		if (!settings.groupMCMs)
			ImGuiMCP::TextDisabled("Enable the MCMs folder to use alphabetical subfolders.");
		if (!ImGuiMCP::CollapsingHeader("Edit alphabetical ranges"))
			return;
		static std::string loaded;
		static std::string draft;
		static int         groups = 5;
		if (loaded != settings.mcmRangeEnds) {
			loaded = draft = settings.mcmRangeEnds;
			groups = static_cast<int>(draft.size());
		}
		std::vector<std::string> names;
		const auto               snapshot = BridgeController::GetSingleton().Snapshot();
		for (const auto& mod : snapshot->mods)
			names.push_back(ResolveMCMAlias(settings, mod.stableID, mod.displayName));
		ImGuiMCP::TextWrapped("Choose each range's end letter. The next range starts automatically; gaps and overlaps are prevented. Empty folders are not shown.");
		ImGuiMCP::SliderInt("Number of balanced groups", &groups, 1, 26);
		if (ImGuiMCP::Button("Distribute evenly"))
			draft = BalanceMCMRanges(names, groups);
		ImGuiMCP::SameLine();
		if (ImGuiMCP::Button("Default ranges"))
			draft = "CGLRZ";
		for (std::size_t i = 0; i < draft.size(); ++i) {
			ImGuiMCP::PushID(static_cast<int>(i));
			const char               start = i == 0 ? 'A' : static_cast<char>(draft[i - 1] + 1);
			const auto               range = MCMNameRange(std::string(1, start), draft);
			std::vector<std::string> matches;
			for (const auto& name : names)
				if (MCMNameRange(name, draft) == range)
					matches.push_back(name);
			ImGuiMCP::Text("%s (%zu MCMs)", range.c_str(), matches.size());
			ImGuiMCP::SameLine();
			ImGuiMCP::SetNextItemWidth(90);
			ImGuiMCP::BeginDisabled(i + 1 == draft.size());
			if (ImGuiMCP::BeginCombo("End", std::string(1, draft[i]).c_str())) {
				const char limit = i + 1 == draft.size() ? 'Z' : static_cast<char>(draft[i + 1] - 1);
				for (char letter = start; letter <= limit; ++letter) {
					if (ImGuiMCP::Selectable(std::string(1, letter).c_str(), draft[i] == letter))
						draft[i] = letter;
				}
				ImGuiMCP::EndCombo();
			}
			ImGuiMCP::EndDisabled();
			ImGuiMCP::SameLine();
			ImGuiMCP::BeginDisabled(start == draft[i]);
			const bool split = ImGuiMCP::Button("Split");
			ImGuiMCP::EndDisabled();
			ImGuiMCP::SameLine();
			ImGuiMCP::BeginDisabled(draft.size() == 1);
			const bool remove = ImGuiMCP::Button("Merge");
			ImGuiMCP::EndDisabled();
			if (ImGuiMCP::TreeNode("Preview")) {
				std::ranges::sort(matches, {}, [](const auto& a_name) { return MCMNameSortKey(a_name); });
				for (const auto& name : matches) ImGuiMCP::TextUnformatted(name.c_str());
				ImGuiMCP::TreePop();
			}
			ImGuiMCP::PopID();
			if (split) {
				draft.insert(i, 1, static_cast<char>((start + draft[i]) / 2));
				break;
			}
			if (remove) {
				draft.erase(i + 1 == draft.size() ? i - 1 : i, 1);
				break;
			}
		}
		const auto other = std::ranges::count_if(names, [&](const auto& a_name) { return MCMNameRange(a_name, draft) == "0-9 & Other"; });
		ImGuiMCP::Text("0-9 & Other: %td MCMs (including letters outside A-Z)", other);
		ImGuiMCP::TextWrapped("German umlauts sort with A/O/U; other non-A-Z initials use Other. Balancing keeps whole letters together and runs only on request.");
		if (ImGuiMCP::Button("Apply ranges"))
			service.SetMCMRanges(settings.alphabeticMCMs, draft);
		ImGuiMCP::SameLine();
		if (ImGuiMCP::Button("Discard edits"))
			draft = settings.mcmRangeEnds;
	}
}
