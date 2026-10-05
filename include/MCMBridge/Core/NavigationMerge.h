#pragma once

#include "MCMBridge/Core/ClassicPageList.h"
#include "MCMBridge/Core/Interfaces.h"
#include "MCMBridge/Core/Model.h"
#include "MCMBridge/Core/StableId.h"

#include <algorithm>

namespace MCMBridge
{
	inline MCMSnapshot PrepareRegistryRefresh(const MCMSnapshot& a_previous, std::span<const MCMDescriptor> a_registry)
	{
		MCMSnapshot result;
		result.refreshing = true;
		for (const auto& entry : a_registry) {
			const auto previous = std::ranges::find(a_previous.mods, entry.stableID, &MCMMod::stableID);
			MCMMod     mod = previous != a_previous.mods.end() ? *previous : MCMMod{};
			mod.stableID = entry.stableID;
			mod.displayName = entry.displayName;
			mod.backend = entry.backend;
			mod.ownerPlugin = entry.ownerPlugin;
			mod.questFormID = entry.questFormID;
			mod.scriptName = entry.scriptName;
			mod.interopID = entry.interopID;
			mod.pageScopedState = entry.pageScopedState;
			result.mods.push_back(std::move(mod));
		}
		return result;
	}

	inline void AppendNavigationStructure(std::vector<std::string>& a_key, const MCMMod& a_mod, std::string_view a_displayName)
	{
		a_key.push_back(a_mod.stableID);
		a_key.emplace_back(a_displayName);
		a_key.push_back(std::to_string(a_mod.pages.size()));
		for (const auto& page : a_mod.pages) {
			a_key.push_back(page.stableID);
			a_key.push_back(page.displayName);
		}
	}

	inline const MCMPage* ResolveNavigationSelection(const MCMMod& a_mod, std::string_view a_id, std::string_view a_rawName)
	{
		const auto exact = std::ranges::find(a_mod.pages, a_id, &MCMPage::stableID);
		if (exact != a_mod.pages.end())
			return &*exact;
		const MCMPage* match{};
		for (const auto& page : a_mod.pages) {
			if (page.rawName != a_rawName)
				continue;
			if (match)
				return nullptr;
			match = &page;
		}
		return match;
	}

	struct NavigationRecovery
	{
		bool attempted{};
		bool blocked{};
		bool Begin()
		{
			if (attempted) {
				blocked = true;
				return false;
			}
			attempted = true;
			return true;
		}
	};

	inline bool MergeNavigation(MCMMod& a_mod, std::span<const std::string> a_names)
	{
		std::optional<ClassicPageSelection> opening;
		if (const auto found = std::ranges::find(a_mod.pages, -1, &MCMPage::index); found != a_mod.pages.end() && !found->openingPlaceholder) {
			opening = ClassicPageSelection{ found->rawName, found->index };
		}
		const auto selections = BuildClassicPageList(a_names, opening);
		if (std::ranges::equal(a_mod.pages, selections, [](const auto& a_page, const auto& a_selection) {
				return a_page.rawName == a_selection.name && a_page.index == a_selection.index;
			}))
			return false;

		std::vector<MCMPage> pages;
		for (const auto& selection : selections) {
			const auto id = MakeClassicPageID(a_mod.stableID, selection.name, selection.index);
			const auto old = std::ranges::find_if(a_mod.pages, [&](const auto& a_page) {
				return a_page.stableID == id && a_page.rawName == selection.name && a_page.index == selection.index;
			});
			if (old != a_mod.pages.end()) {
				pages.push_back(*old);
			} else {
				MCMPage page;
				page.stableID = id;
				page.rawName = selection.name;
				page.displayName = selection.name;
				page.index = selection.index;
				pages.push_back(std::move(page));
			}
		}
		a_mod.pages = std::move(pages);
		return true;
	}

	inline const MCMPage* MergeScriptNavigation(MCMMod& a_mod, std::span<const std::string> a_names, std::string_view a_rawPage, bool& a_changed, bool a_opening = false)
	{
		a_changed = false;
		if (a_opening) {
			if (!a_rawPage.empty())
				return nullptr;
			auto opening = std::ranges::find(a_mod.pages, -1, &MCMPage::index);
			if (opening == a_mod.pages.end()) {
				MCMPage page;
				page.index = -1;
				page.stableID = MakeClassicPageID(a_mod.stableID, "", -1);
				a_mod.pages.insert(a_mod.pages.begin(), std::move(page));
				a_changed = true;
			} else {
				if (!opening->rawName.empty()) {
					*opening = MCMPage{};
					opening->index = -1;
					opening->stableID = MakeClassicPageID(a_mod.stableID, "", -1);
					a_changed = true;
				}
				opening->openingPlaceholder = false;
			}
			a_changed = MergeNavigation(a_mod, a_names) || a_changed;
			const auto selected = std::ranges::find(a_mod.pages, -1, &MCMPage::index);
			return selected == a_mod.pages.end() ? nullptr : std::addressof(*selected);
		}
		if (!IsSelectableClassicPageName(a_rawPage) || std::ranges::count(a_names, a_rawPage) != 1)
			return nullptr;
		const auto index = std::ranges::find(a_names, a_rawPage) - a_names.begin();
		a_changed = MergeNavigation(a_mod, a_names);
		const auto selected = std::ranges::find_if(a_mod.pages, [&](const auto& a_page) {
			return a_page.rawName == a_rawPage && a_page.index == index;
		});
		return selected == a_mod.pages.end() ? nullptr : &*selected;
	}
}
