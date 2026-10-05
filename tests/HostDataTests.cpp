#include "MCMBridge/API/HostData.h"

#include <catch2/catch_test_macros.hpp>

using namespace MCMBridge;

TEST_CASE("Native host data owns more than 2000 registrations without truncation")
{
	HostDataInput input;
	input.session = 3;
	for (std::uint32_t i = 0; i < 2049; ++i) {
		MCMDescriptor descriptor;
		descriptor.interopID = "Script::Mod " + std::to_string(i);
		descriptor.displayName = "Alias";
		input.registry.push_back(std::move(descriptor));
	}
	HostData   data(std::move(input));
	const auto view = data.View();
	REQUIRE(view.mod_count == 2049);
	CHECK(view.session == 3);
	for (std::uint32_t i = 0; i < view.mod_count; ++i) {
		CHECK(std::string(view.mods[i].name) == "Mod " + std::to_string(i));
		CHECK(view.mods[i].navigation_ready == 0);
	}
}

TEST_CASE("Native host data distinguishes unread pages from confirmed empty pages")
{
	HostDataInput input;
	MCMDescriptor descriptor;
	descriptor.interopID = "Script::Mod";
	input.registry.push_back(descriptor);
	input.active.emplace();
	input.active->interopID = descriptor.interopID;
	MCMPage page;
	page.index = 0;
	page.rawName = "Page (1/2)";
	input.active->pages.push_back(page);
	page.index = 1;
	page.rawName = "Page (2/2)";
	input.active->pages.push_back(page);
	input.currentPage = 1;
	HostData    data(std::move(input));
	const auto& mod = data.View().mods[0];
	REQUIRE(mod.page_count == 2);
	CHECK(mod.navigation_ready == 1);
	CHECK(mod.pages[0].controls_ready == 0);
	CHECK(mod.pages[1].controls_ready == 1);
	CHECK(mod.pages[1].control_count == 0);
	CHECK(std::string(mod.pages[1].name) == "Page (2/2)");
}

TEST_CASE("Native host data retains prepared dialog strings independently of input lifetime")
{
	HostDataInput input;
	MCMDescriptor descriptor;
	descriptor.interopID = "Script::Mod";
	input.registry.push_back(descriptor);
	input.active.emplace();
	input.active->interopID = descriptor.interopID;
	MCMPage page;
	page.index = -1;
	MCMControl control;
	control.type = MCMControlType::kMenu;
	control.rawLabel = "$LABEL";
	control.menu.emplace();
	control.menu->availability = MetadataAvailability::kAvailable;
	control.menu->options = { "$ONE", "$TWO" };
	control.menu->selectedIndex = 1;
	control.menu->defaultIndex = 0;
	page.controls.push_back(control);
	input.active->pages.push_back(page);
	input.currentPage = -1;
	HostData data(input);
	input = {};
	const auto& view = data.View().mods[0].pages[0].controls[0];
	CHECK(view.dialog_ready == 1);
	REQUIRE(view.menu_count == 2);
	CHECK(std::string(view.menu_items[1]) == "$TWO");
	CHECK(view.menu_index == 1);
	CHECK(view.menu_default == 0);
	CHECK(std::string(view.label) == "$LABEL");
}
