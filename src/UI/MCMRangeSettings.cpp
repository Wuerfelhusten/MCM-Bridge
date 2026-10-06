#include "MCMBridge/UI/MCMRangeSettings.h"

#include "MCMBridge/Core/MCMOrganization.h"
#include "MCMBridge/Plugin/BridgeController.h"
#include "MCMBridge/Plugin/BridgeSettingsService.h"
#include "MCMBridge/UI/FrontendUI.h"

namespace MCMBridge::MCMRangeSettings
{
	void Render()
	{
		auto& service = BridgeSettingsService::GetSingleton();
		auto  settings = service.Get();
		BridgeUI::Separator();
		if (BridgeUI::Checkbox("Group MCMs in an MCMs folder", &settings.groupMCMs))
			service.SetGroupMCMs(settings.groupMCMs);
		BridgeUI::BeginDisabled(!settings.groupMCMs);
		if (BridgeUI::Checkbox("Alphabetical subfolders", &settings.alphabeticMCMs))
			service.SetMCMRanges(settings.alphabeticMCMs, settings.mcmRangeEnds);
		BridgeUI::EndDisabled();
		if (!settings.groupMCMs)
			BridgeUI::TextDisabled("Enable the MCMs folder to use alphabetical subfolders.");
		if (!BridgeUI::CollapsingHeader("Edit alphabetical ranges"))
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
		BridgeUI::TextWrapped("Choose each range's end letter. The next range starts automatically; gaps and overlaps are prevented. Empty folders are not shown.");
		BridgeUI::SliderInt("Number of balanced groups", &groups, 1, 26);
		if (BridgeUI::Button("Distribute evenly"))
			draft = BalanceMCMRanges(names, groups);
		BridgeUI::SameLine();
		if (BridgeUI::Button("Default ranges"))
			draft = "CGLRZ";
		for (std::size_t i = 0; i < draft.size(); ++i) {
			BridgeUI::PushID(static_cast<int>(i));
			const char               start = i == 0 ? 'A' : static_cast<char>(draft[i - 1] + 1);
			const auto               range = MCMNameRange(std::string(1, start), draft);
			std::vector<std::string> matches;
			for (const auto& name : names)
				if (MCMNameRange(name, draft) == range)
					matches.push_back(name);
			BridgeUI::Text("%s (%zu MCMs)", range.c_str(), matches.size());
			BridgeUI::SameLine();
			BridgeUI::SetNextItemWidth(90);
			BridgeUI::BeginDisabled(i + 1 == draft.size());
			if (BridgeUI::BeginCombo("End", std::string(1, draft[i]).c_str())) {
				const char limit = i + 1 == draft.size() ? 'Z' : static_cast<char>(draft[i + 1] - 1);
				for (char letter = start; letter <= limit; ++letter) {
					if (BridgeUI::Selectable(std::string(1, letter).c_str(), draft[i] == letter))
						draft[i] = letter;
				}
				BridgeUI::EndCombo();
			}
			BridgeUI::EndDisabled();
			BridgeUI::SameLine();
			BridgeUI::BeginDisabled(start == draft[i]);
			const bool split = BridgeUI::Button("Split");
			BridgeUI::EndDisabled();
			BridgeUI::SameLine();
			BridgeUI::BeginDisabled(draft.size() == 1);
			const bool remove = BridgeUI::Button("Merge");
			BridgeUI::EndDisabled();
			if (BridgeUI::TreeNode("Preview")) {
				std::ranges::sort(matches, {}, [](const auto& a_name) { return MCMNameSortKey(a_name); });
				for (const auto& name : matches) BridgeUI::TextUnformatted(name.c_str());
				BridgeUI::TreePop();
			}
			BridgeUI::PopID();
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
		BridgeUI::Text("0-9 & Other: %td MCMs (including letters outside A-Z)", other);
		BridgeUI::TextWrapped("German umlauts sort with A/O/U; other non-A-Z initials use Other. Balancing keeps whole letters together and runs only on request.");
		if (BridgeUI::Button("Apply ranges"))
			service.SetMCMRanges(settings.alphabeticMCMs, draft);
		BridgeUI::SameLine();
		if (BridgeUI::Button("Discard edits"))
			draft = settings.mcmRangeEnds;
	}
}
