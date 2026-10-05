#include "MCMBridge/Framework/MCMEntryRegistry.h"

#include "MCMBridge/Core/FrameworkMenuPath.h"
#include "MCMBridge/Core/MCMOrganization.h"
#include "MCMBridge/Core/NavigationMerge.h"
#include "MCMBridge/Plugin/BridgeSettingsService.h"
#include "MCMBridge/UI/MCMWindow.h"
#include "SKSEMenuFramework.h"

#include <unordered_set>

namespace
{
	std::string DisplayLabel(std::string_view a_source, std::string_view a_fallback)
	{
		std::string result(a_source.empty() ? a_fallback : a_source);
		std::ranges::replace(result, '#', ' ');
		return result;
	}

	void RenderEntry(std::size_t a_slot)
	{
		MCMBridge::MCMEntryRegistry::GetSingleton().Render(a_slot);
	}
}

namespace MCMBridge
{
	MCMEntryRegistry::MCMEntryRegistry() : renderers(RenderEntry) {}

	MCMEntryRegistry& MCMEntryRegistry::GetSingleton()
	{
		static MCMEntryRegistry singleton;
		return singleton;
	}

	void MCMEntryRegistry::Synchronize(std::span<const MCMMod> a_mods)
	{
		const auto               preferences = BridgeSettingsService::GetSingleton().Get();
		const std::scoped_lock   lock(mutex);
		std::vector<std::string> structureKey;
		structureKey.emplace_back(preferences.groupMCMs ? "grouped" : "root");
		structureKey.emplace_back(preferences.alphabeticMCMs ? preferences.mcmRangeEnds : "");
		std::vector<const MCMMod*>                   ordered;
		std::unordered_map<std::string, std::string> names;
		std::unordered_map<std::string, std::string> ranges;
		for (const auto& mod : a_mods) {
			const auto name = ResolveMCMAlias(preferences, mod.stableID, mod.displayName);
			names.emplace(mod.stableID, MCMNameSortKey(name));
			ranges.emplace(mod.stableID, preferences.groupMCMs && preferences.alphabeticMCMs ? MCMNameRange(name, preferences.mcmRangeEnds) : "");
			ordered.push_back(&mod);
			AppendNavigationStructure(structureKey, mod, name);
		}
		if (structureValid && structureKey == structure)
			return;
		structureValid = false;
		bool synchronized = true;
		std::ranges::sort(ordered, [&](const auto* a_left, const auto* a_right) {
			return std::tie(ranges.at(a_left->stableID), names.at(a_left->stableID), a_left->stableID) <
			       std::tie(ranges.at(a_right->stableID), names.at(a_right->stableID), a_right->stableID);
		});
		std::unordered_map<std::string, std::unordered_set<std::string>> usedLabels;
		std::unordered_map<std::string, std::string>                     sectionLabels;
		for (const auto* mod : ordered) {
			auto label = DisplayLabel(ResolveMCMAlias(preferences, mod->stableID, mod->displayName), "Unnamed MCM");
			if (!preferences.groupMCMs)
				label = UppercaseMCMRootInitial(label);
			sectionLabels.emplace(
				mod->stableID, mod->pages.empty() ? label : UniqueFrameworkMenuLabel(label, usedLabels[ranges.at(mod->stableID)]));
		}
		std::vector<std::string> nextPlacement;
		for (const auto* mod : ordered) {
			if (mod->pages.empty())
				continue;
			nextPlacement.push_back(mod->stableID);
			nextPlacement.push_back(sectionLabels.at(mod->stableID));
			nextPlacement.push_back(ranges.at(mod->stableID));
		}
		// Subfolders preserve insertion order. Rebuild only on placement changes,
		// never for values or page refreshes; renderer slots remain stable.
		if (preferences.groupMCMs && folderCreated && placement != nextPlacement) {
			if (!SKSEMenuFramework::DeleteSection(std::string(mcmFolderPath)))
				return;
			folderCreated = false;
			for (auto& [id, mod] : mods) {
				if (!mod.grouped)
					continue;
				mod.active = false;
				for (auto& entry : entries)
					if (entry.modID == id)
						entry.active = false;
			}
		}
		std::unordered_set<std::string> desiredModIDs;
		desiredModIDs.reserve(a_mods.size());
		for (const auto& mod : a_mods) {
			desiredModIDs.insert(mod.stableID);
		}
		// Free all changed root names before adding their replacements. Two aliases
		// may exchange names, so renaming either section first would collide.
		if (!preferences.groupMCMs) {
			for (auto& [modID, registeredMod] : mods) {
				if (!registeredMod.active || registeredMod.grouped || !desiredModIDs.contains(modID) ||
					registeredMod.sectionSegment == sectionLabels.at(modID))
					continue;
				if (!SKSEMenuFramework::DeleteSection(MCMFrameworkSectionPath(registeredMod.sectionSegment, false))) {
					synchronized = false;
					continue;
				}
				registeredMod.active = false;
				for (auto& entry : entries)
					if (entry.modID == modID)
						entry.active = false;
			}
		}

		for (auto& [modID, registeredMod] : mods) {
			if (!registeredMod.active || (desiredModIDs.contains(modID) && registeredMod.grouped == preferences.groupMCMs)) {
				continue;
			}
			// Moving a section reuses renderer slots and identities; only its presentation is rebuilt.
			if (SKSEMenuFramework::DeleteSection(MCMFrameworkSectionPath(registeredMod.sectionSegment, registeredMod.grouped, registeredMod.range))) {
				registeredMod.active = false;
				for (auto& entry : entries) {
					if (entry.modID == modID) {
						entry.active = false;
					}
				}
			} else {
				synchronized = false;
			}
		}

		for (const auto* modPointer : ordered) {
			const auto& mod = *modPointer;
			const auto& sectionLabel = sectionLabels.at(mod.stableID);
			const auto& desiredSection = sectionLabel;
			auto&       registeredMod = mods[mod.stableID];
			if (registeredMod.active && registeredMod.grouped != preferences.groupMCMs) {
				continue;
			}
			if (registeredMod.active && registeredMod.sectionSegment != desiredSection) {
				if (SKSEMenuFramework::RenameSection(
						MCMFrameworkSectionPath(registeredMod.sectionSegment, registeredMod.grouped, registeredMod.range),
						EscapeFrameworkMenuPathSegment(desiredSection))) {
					registeredMod.sectionSegment = desiredSection;
					for (auto& entry : entries) {
						if (entry.modID == mod.stableID && entry.active) {
							entry.sectionSegment = desiredSection;
						}
					}
				} else {
					synchronized = false;
				}
			} else if (!registeredMod.active) {
				registeredMod.sectionSegment = desiredSection;
				registeredMod.grouped = preferences.groupMCMs;
				registeredMod.range = ranges.at(mod.stableID);
			}

			std::vector<std::pair<const MCMPage*, std::string>> desiredPages;
			desiredPages.reserve(mod.pages.size());
			std::unordered_set<std::string> usedPageLabels;
			std::unordered_set<std::string> desiredPageIDs;
			for (const auto& page : mod.pages) {
				const auto  baseLabel = DisplayLabel(page.displayName, "General");
				auto        pageLabel = baseLabel;
				std::size_t suffix = 2;
				while (!usedPageLabels.insert(pageLabel).second) {
					pageLabel = std::format("{} ({})", baseLabel, suffix++);
				}
				desiredPageIDs.insert(page.stableID);
				desiredPages.emplace_back(std::addressof(page), std::move(pageLabel));
			}

			for (auto& entry : entries) {
				if (entry.modID != mod.stableID || !entry.active || desiredPageIDs.contains(entry.pageID)) {
					continue;
				}
				// A moved page keeps its renderer slot only when its raw name is unique on both sides.
				if (std::ranges::count(mod.pages, entry.rawName, &MCMPage::rawName) == 1 &&
					std::ranges::count_if(entries, [&](const auto& a_entry) { return a_entry.active && a_entry.modID == mod.stableID && a_entry.rawName == entry.rawName; }) == 1) {
					const auto page = std::ranges::find(mod.pages, entry.rawName, &MCMPage::rawName);
					const auto oldKey = std::format("{}:{}", mod.stableID, entry.pageID);
					const auto slot = slotsByID.at(oldKey);
					slotsByID.erase(oldKey);
					entry.pageID = page->stableID;
					slotsByID[std::format("{}:{}", mod.stableID, entry.pageID)] = slot;
					continue;
				}
				if (SKSEMenuFramework::DeleteSection(JoinFrameworkMenuPath(entry.sectionSegment, entry.pageLabel, registeredMod.grouped, registeredMod.range))) {
					entry.active = false;
				} else {
					synchronized = false;
				}
			}

			if (desiredPages.empty()) {
				if (registeredMod.active &&
					SKSEMenuFramework::DeleteSection(MCMFrameworkSectionPath(registeredMod.sectionSegment, registeredMod.grouped, registeredMod.range))) {
					registeredMod.active = false;
				}
				if (registeredMod.active)
					synchronized = false;
				continue;
			}

			for (const auto& [page, pageLabel] : desiredPages) {
				const auto registrationID = std::format("{}:{}", mod.stableID, page->stableID);
				const auto existing = slotsByID.find(registrationID);
				if (existing != slotsByID.end()) {
					auto& entry = entries[existing->second];
					if (entry.active && entry.pageLabel != pageLabel &&
						SKSEMenuFramework::RenameSection(
							JoinFrameworkMenuPath(entry.sectionSegment, entry.pageLabel, registeredMod.grouped, registeredMod.range),
							EscapeFrameworkMenuPathSegment(pageLabel))) {
						entry.pageLabel = pageLabel;
					}
					if (entry.active && entry.pageLabel != pageLabel)
						synchronized = false;
					if (!entry.active) {
						entry.pageLabel = pageLabel;
						entry.sectionSegment = registeredMod.sectionSegment;
						entry.active = true;
						SKSEMenuFramework::SetSection(MCMFrameworkSectionPath(entry.sectionSegment, registeredMod.grouped, registeredMod.range));
						SKSEMenuFramework::AddSectionItem(
							EscapeFrameworkMenuPathSegment(entry.pageLabel), renderers.Get(existing->second));
						registeredMod.active = true;
						folderCreated |= registeredMod.grouped;
					}
					continue;
				}

				const auto                  slot = entries.size();
				RendererCallbacks::Callback renderer;
				try {
					renderer = renderers.Get(slot);
				} catch (const std::exception& error) {
					SKSE::log::error("Cannot allocate MCM page renderer {}: {}", slot, error.what());
					return;
				}
				entries.push_back({ mod.stableID, page->stableID, pageLabel, registeredMod.sectionSegment, true, page->rawName });
				slotsByID.emplace(registrationID, slot);
				SKSEMenuFramework::SetSection(MCMFrameworkSectionPath(registeredMod.sectionSegment, registeredMod.grouped, registeredMod.range));
				SKSEMenuFramework::AddSectionItem(EscapeFrameworkMenuPathSegment(pageLabel), renderer);
				registeredMod.active = true;
				folderCreated |= registeredMod.grouped;
				const auto visibleControls = std::ranges::count_if(page->controls, [](const auto& a_control) {
					return !a_control.hidden && a_control.type != MCMControlType::kEmpty;
				});
				SKSE::log::info(
					"Registered MCM page entry {}/{}: raw=\"{}\" index={} controls={} custom_content={}",
					sectionLabel, pageLabel, page->rawName, page->index, visibleControls, page->customContent.has_value());
			}
		}
		if (folderCreated && std::ranges::none_of(mods, [](const auto& a_mod) { return a_mod.second.active && a_mod.second.grouped; })) {
			SKSEMenuFramework::DeleteSection(std::string(mcmFolderPath));
			folderCreated = false;
		}
		if (synchronized) {
			structure = std::move(structureKey);
			placement = std::move(nextPlacement);
		}
		structureValid = synchronized;
		SKSE::log::info("Framework navigation structure synchronized: mods={}", a_mods.size());
	}

	void MCMEntryRegistry::Render(std::size_t a_slot) const
	{
		Entry entry;
		{
			const std::scoped_lock lock(mutex);
			if (a_slot >= entries.size() || !entries[a_slot].active) {
				return;
			}
			entry = entries[a_slot];
		}
		MCMWindow::Render(entry.modID, entry.pageID);
	}
}
