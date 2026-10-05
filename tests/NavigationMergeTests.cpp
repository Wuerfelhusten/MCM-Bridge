#include "MCMBridge/Core/NavigationMerge.h"

#include <catch2/catch_test_macros.hpp>

using namespace MCMBridge;

TEST_CASE("Script navigation resolves the current raw name without changing rejected catalogs")
{
	MCMMod mod;
	mod.stableID = "script";
	MergeNavigation(mod, std::vector<std::string>{ "First", "Hall (1/4)" });
	mod.pages[0].title = "confirmed";
	bool        changed{};
	const auto* selected = MergeScriptNavigation(mod, std::vector<std::string>{ "Hall (1/4)", "First" }, "First", changed);
	REQUIRE(selected);
	CHECK(selected->index == 1);
	CHECK(changed);
	CHECK(selected->title.empty());
	selected = MergeScriptNavigation(mod, std::vector<std::string>{ "Hall (1/4)", "First" }, "Hall (1/4)", changed);
	REQUIRE(selected);
	CHECK(selected->index == 0);
	CHECK_FALSE(changed);
	const auto retained = mod.pages[0].stableID;
	CHECK_FALSE(MergeScriptNavigation(mod, std::vector<std::string>{ "First", "First" }, "First", changed));
	CHECK_FALSE(changed);
	CHECK(mod.pages[0].stableID == retained);
	CHECK_FALSE(MergeScriptNavigation(mod, std::vector<std::string>{ "Other" }, "First", changed));
	CHECK(mod.pages[0].stableID == retained);
	CHECK_FALSE(MergeScriptNavigation(mod, std::vector<std::string>{ "$Page" }, "Translated Page", changed));
	CHECK_FALSE(MergeScriptNavigation(mod, std::vector<std::string>{ "", "Other" }, "", changed));
	CHECK_FALSE(MergeScriptNavigation(mod, std::vector<std::string>{ "None", "Other" }, "None", changed));
	CHECK_FALSE(changed);
	CHECK(mod.pages[0].stableID == retained);
}

TEST_CASE("Registry refresh preserves confirmed pages until replacements arrive")
{
	MCMSnapshot previous;
	MCMMod      mod;
	mod.stableID = "one";
	MergeNavigation(mod, std::vector<std::string>{ "General", "Second" });
	mod.pages[0].controls.push_back(MCMControl{});
	previous.mods.push_back(mod);
	mod.stableID = "removed";
	previous.mods.push_back(mod);
	MCMDescriptor existing;
	existing.stableID = "one";
	MCMDescriptor added;
	added.stableID = "new";
	const auto refreshed = PrepareRegistryRefresh(previous, std::vector{ existing, added });
	CHECK(refreshed.refreshing);
	REQUIRE(refreshed.mods.size() == 2);
	CHECK(refreshed.mods[0].pages.size() == 2);
	CHECK(refreshed.mods[0].pages[0].controls.size() == 1);
	CHECK(refreshed.mods[1].pages.empty());
	CHECK(previous.mods.size() == 2);
}

TEST_CASE("Framework structure ignores values but detects page changes and aliases")
{
	MCMMod mod;
	mod.stableID = "one";
	MergeNavigation(mod, std::vector<std::string>{ "General", "Second" });
	std::vector<std::string> before;
	AppendNavigationStructure(before, mod, "Name");
	mod.pages[0].controls.push_back(MCMControl{});
	std::vector<std::string> values;
	AppendNavigationStructure(values, mod, "Name");
	CHECK(values == before);
	std::vector<std::string> alias;
	AppendNavigationStructure(alias, mod, "Alias");
	CHECK(alias != before);
	std::swap(mod.pages[0], mod.pages[1]);
	std::vector<std::string> moved;
	AppendNavigationStructure(moved, mod, "Name");
	CHECK(moved != before);
}

TEST_CASE("Navigation recovery retries once and blocks repeated stale selections")
{
	NavigationRecovery recovery;
	CHECK(recovery.Begin());
	CHECK_FALSE(recovery.Begin());
	CHECK(recovery.blocked);
	recovery = {};
	CHECK(recovery.Begin());
}

TEST_CASE("Selection remapping never substitutes a page at the old index")
{
	MCMMod mod;
	mod.stableID = "test";
	MergeNavigation(mod, std::vector<std::string>{ "First", "Second" });
	const auto oldID = mod.pages[0].stableID;
	MergeNavigation(mod, std::vector<std::string>{ "Second", "First" });
	const auto selected = ResolveNavigationSelection(mod, oldID, "First");
	REQUIRE(selected);
	CHECK(selected->index == 1);
	MergeNavigation(mod, std::vector<std::string>{ "Second" });
	CHECK_FALSE(ResolveNavigationSelection(mod, oldID, "First"));
	MergeNavigation(mod, std::vector<std::string>{ "Second", "First", "First" });
	CHECK_FALSE(ResolveNavigationSelection(mod, oldID, "First"));
}

TEST_CASE("Navigation changes retain only exact page identities")
{
	MCMMod mod;
	mod.stableID = "test";
	const std::vector<std::string> initial{ "General", "Hall (1/3)" };
	REQUIRE(MergeNavigation(mod, initial));
	mod.pages[0].title = "cached";
	mod.pages[1].title = "must not transfer";
	REQUIRE_FALSE(MergeNavigation(mod, initial));
	const std::vector<std::string> updated{ "General", "Hall (1/4)", "Hall (2/4)" };
	REQUIRE(MergeNavigation(mod, updated));
	REQUIRE(mod.pages.size() == 3);
	REQUIRE(mod.pages[0].title == "cached");
	REQUIRE(mod.pages[1].title.empty());
	REQUIRE(mod.pages[1].rawName == "Hall (1/4)");
	REQUIRE(MergeNavigation(mod, std::vector<std::string>{ "Hall (2/4)" }));
	REQUIRE(mod.pages.size() == 1);
	REQUIRE(mod.pages[0].index == 0);
}

TEST_CASE("Navigation polling preserves an explicit opening page")
{
	MCMMod mod;
	mod.stableID = "test";
	MCMPage opening;
	opening.index = -1;
	opening.stableID = MakeClassicPageID(mod.stableID, "", -1);
	opening.title = "landing";
	mod.pages.push_back(opening);
	REQUIRE(MergeNavigation(mod, std::vector<std::string>{ "Settings" }));
	REQUIRE(mod.pages.front().index == -1);
	REQUIRE(mod.pages.front().title == "landing");
	REQUIRE_FALSE(MergeNavigation(mod, std::vector<std::string>{ "Settings" }));
}

TEST_CASE("Script rename selects the new raw page without retaining old controls")
{
	MCMMod mod;
	mod.stableID = "module-owner";
	MergeNavigation(mod, std::vector<std::string>{ "Original", "Other" });
	mod.pages[0].controls.push_back(MCMControl{});
	bool        changed{};
	const auto* selected = MergeScriptNavigation(mod, std::vector<std::string>{ "Renamed/Module", "Other" }, "Renamed/Module", changed);
	REQUIRE(selected);
	CHECK(changed);
	CHECK(selected->rawName == "Renamed/Module");
	CHECK(selected->controls.empty());
	CHECK_FALSE(MergeScriptNavigation(mod, std::vector<std::string>{ "Renamed/Module", "Other" }, "Original", changed));
	selected = MergeScriptNavigation(mod, std::vector<std::string>{ "Renamed/Again", "Other" }, "Other", changed);
	REQUIRE(selected);
	CHECK(selected->rawName == "Other");
	CHECK(changed);
}

TEST_CASE("Explicit script opening requests preserve named navigation")
{
	MCMMod mod;
	mod.stableID = "opening-owner";
	MergeNavigation(mod, std::vector<std::string>{ "First", "Second" });
	bool changed{};
	CHECK_FALSE(MergeScriptNavigation(mod, std::vector<std::string>{ "First", "Second" }, "", changed));
	const auto* opening = MergeScriptNavigation(mod, std::vector<std::string>{ "First", "Second" }, "", changed, true);
	REQUIRE(opening);
	CHECK(changed);
	CHECK(opening->index == -1);
	CHECK(opening->rawName.empty());
	CHECK_FALSE(opening->openingPlaceholder);
	CHECK(mod.pages.size() == 3);
	const auto openingID = opening->stableID;
	mod.pages.front().title = "Confirmed opening content";
	opening = MergeScriptNavigation(mod, std::vector<std::string>{ "First", "Second" }, "", changed, true);
	REQUIRE(opening);
	CHECK_FALSE(changed);
	CHECK(opening->stableID == openingID);
	CHECK(opening->title == "Confirmed opening content");
	opening = MergeScriptNavigation(mod, std::vector<std::string>{ "Second" }, "", changed, true);
	REQUIRE(opening);
	CHECK(changed);
	CHECK(mod.pages.size() == 2);
	CHECK_FALSE(MergeScriptNavigation(mod, std::vector<std::string>{ "Second" }, "Second", changed, true));
	CHECK_FALSE(changed);
}
