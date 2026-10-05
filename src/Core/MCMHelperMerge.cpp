#include "MCMBridge/Core/MCMHelperMerge.h"

#include "MCMBridge/Core/StableId.h"

#include <algorithm>

namespace
{
	bool Compatible(MCMBridge::MCMControlType a_parsed, MCMBridge::MCMControlType a_live)
	{
		return a_parsed == a_live || (a_parsed == MCMBridge::MCMControlType::kStepper && a_live == MCMBridge::MCMControlType::kText);
	}

	const MCMBridge::MCMPage* FindPage(const MCMBridge::MCMMod& a_mod, const MCMBridge::MCMPage& a_livePage)
	{
		const auto found = std::ranges::find_if(a_mod.pages, [&](const auto& a_page) {
			return a_page.index == a_livePage.index || (!a_livePage.rawName.empty() && a_page.rawName == a_livePage.rawName);
		});
		return found != a_mod.pages.end() ? std::addressof(*found) : nullptr;
	}

	std::optional<std::size_t> FindControl(
		const MCMBridge::MCMPage&    a_page,
		const MCMBridge::MCMControl& a_live,
		const std::vector<bool>&     a_consumed)
	{
		auto findUnique = [&](const auto& a_predicate) -> std::optional<std::size_t> {
			std::optional<std::size_t> result;
			for (std::size_t index = 0; index < a_page.controls.size(); ++index) {
				if (a_consumed[index] || a_page.controls[index].hidden ||
					!Compatible(a_page.controls[index].type, a_live.type) || !a_predicate(a_page.controls[index]))
					continue;
				if (result)
					return std::nullopt;
				result = index;
			}
			return result;
		};
		if (auto result = findUnique([&](const auto& a_control) {
				return a_control.identity.optionIndex == a_live.identity.optionIndex &&
			           (a_control.label.empty() || a_live.label.empty() || a_control.label == a_live.label);
			}))
			return result;
		if (auto result = findUnique([&](const auto& a_control) {
				return !a_control.label.empty() && a_control.label == a_live.label;
			}))
			return result;
		if (auto result = findUnique([&](const auto& a_control) {
				return a_control.identity.optionIndex == a_live.identity.optionIndex;
			}))
			return result;
		return findUnique([](const auto&) { return true; });
	}

	std::int32_t FindDisplayedOption(const MCMBridge::MCMControl& a_live, const MCMBridge::MenuMetadata& a_menu)
	{
		const auto* displayed = std::get_if<std::string>(std::addressof(a_live.value));
		if (!displayed)
			return -1;
		const auto option = std::ranges::find(a_menu.options, *displayed);
		if (option != a_menu.options.end())
			return static_cast<std::int32_t>(std::distance(a_menu.options.begin(), option));
		const auto shortName = std::ranges::find(a_menu.shortNames, *displayed);
		return shortName == a_menu.shortNames.end() ? -1 :
		                                              static_cast<std::int32_t>(std::distance(a_menu.shortNames.begin(), shortName));
	}

	void MergeMenu(MCMBridge::MCMControl& a_live, const MCMBridge::MCMControl& a_parsed)
	{
		if (!a_parsed.menu)
			return;
		if (const auto* displayed = std::get_if<std::string>(std::addressof(a_live.value)))
			a_live.displayValue = *displayed;
		auto selected = a_live.menu ? a_live.menu->selectedIndex : FindDisplayedOption(a_live, *a_parsed.menu);
		if (selected < 0) {
			if (const auto* configured = std::get_if<std::int32_t>(std::addressof(a_parsed.value)))
				selected = *configured;
		}
		if (!a_live.menu || a_live.menu->options.empty()) {
			a_live.menu = a_parsed.menu;
		} else {
			a_live.menu->defaultIndex = a_parsed.menu->defaultIndex;
		}
		a_live.menu->selectedIndex = selected;
		a_live.value = selected;
		if (a_parsed.type == MCMBridge::MCMControlType::kStepper)
			a_live.type = MCMBridge::MCMControlType::kStepper;
	}

	void SetCapability(MCMBridge::MCMControl& a_control)
	{
		using MCMBridge::MCMControlType;
		using MCMBridge::WriteCapability;
		if (a_control.disabled) {
			a_control.writeCapability = WriteCapability::kDisabled;
			return;
		}
		if ((a_control.type == MCMControlType::kMenu || a_control.type == MCMControlType::kStepper) &&
			(!a_control.menu || a_control.menu->options.empty())) {
			a_control.writeCapability = WriteCapability::kMissingOptions;
			return;
		}
		const auto valueControl = a_control.type == MCMControlType::kToggle || a_control.type == MCMControlType::kSlider ||
		                          a_control.type == MCMControlType::kMenu || a_control.type == MCMControlType::kStepper ||
		                          a_control.type == MCMControlType::kColor || a_control.type == MCMControlType::kKeymap ||
		                          a_control.type == MCMControlType::kInput;
		a_control.writeCapability = valueControl || (a_control.type == MCMControlType::kText && a_control.action) ?
		                                WriteCapability::kWritable :
		                                WriteCapability::kReadOnly;
	}
}

namespace MCMBridge
{
	void MergeMCMHelperMetadata(MCMMod& a_liveMod, const MCMMod& a_parsedMod)
	{
		a_liveMod.backend = MCMBackendKind::kMCMHelper;
		a_liveMod.displayName = a_parsedMod.displayName;
		a_liveMod.minimumMCMHelperVersion = a_parsedMod.minimumMCMHelperVersion;
		a_liveMod.pluginRequirements = a_parsedMod.pluginRequirements;
		for (auto& page : a_liveMod.pages) {
			const auto* parsedPage = FindPage(a_parsedMod, page);
			if (!parsedPage)
				continue;
			page.customContent = parsedPage->customContent;
			std::vector<bool> consumed(parsedPage->controls.size());
			for (auto& control : page.controls) {
				const auto match = FindControl(*parsedPage, control, consumed);
				if (!match) {
					control.identity.backend = MCMBackendKind::kMCMHelper;
					SetCapability(control);
					continue;
				}
				consumed[*match] = true;
				const auto& metadata = parsedPage->controls[*match];
				control.identity.explicitID = metadata.identity.explicitID;
				control.identity.stableID = MakeHelperControlID(
					a_liveMod.ownerPlugin, a_liveMod.questFormID, a_liveMod.scriptName, page.rawName,
					control.identity.stateName, control.identity.explicitID);
				control.identity.backend = MCMBackendKind::kMCMHelper;
				control.identity.confidence = metadata.identity.confidence;
				control.help = metadata.help;
				control.source = metadata.source;
				if (!control.defaultValue)
					control.defaultValue = metadata.defaultValue;
				control.action = metadata.action;
				control.condition = metadata.condition;
				control.groupControl = metadata.groupControl;
				control.ignoreConflicts = metadata.ignoreConflicts;
				if (!control.slider || control.slider->availability != MetadataAvailability::kAvailable)
					control.slider = metadata.slider;
				if (!control.color || control.color->availability != MetadataAvailability::kAvailable)
					control.color = metadata.color;
				MergeMenu(control, metadata);
				SetCapability(control);
			}
		}
		for (const auto& parsedPage : a_parsedMod.pages) {
			if (!parsedPage.customContent || FindPage(a_liveMod, parsedPage))
				continue;
			a_liveMod.pages.push_back(parsedPage);
		}
	}
}
