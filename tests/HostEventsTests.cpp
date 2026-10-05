#include "MCMBridge/API/HostEvents.h"

#include <catch2/catch_test_macros.hpp>

using namespace MCMBridge;

TEST_CASE("Native host observers can unsubscribe during delivery")
{
	HostEvents events;
	struct Receiver
	{
		HostEvents*   events;
		std::uint64_t token{};
		std::uint32_t calls{};
	} receiver{ &events };
	REQUIRE(events.Subscribe([](void* a_user, const MCMHostEvent*) {
		auto& target = *static_cast<Receiver*>(a_user);
		++target.calls;
		target.events->Unsubscribe(target.token);
	},
				&receiver, receiver.token) == MCM_HOST_OK);
	events.Session(MCM_HOST_SESSION_BEGIN, 1);
	events.Session(MCM_HOST_SESSION_END, 1);
	CHECK(receiver.calls == 1);
	CHECK(events.Unsubscribe(receiver.token) == MCM_HOST_INVALID_ARGUMENT);
}

TEST_CASE("Native host recording uses raw identities and menu text")
{
	HostEvents events;
	MCMMod     mod;
	mod.interopID = "ExampleScript::Original name";
	mod.displayName = "Frontend alias";
	mod.pageScopedState = true;
	MCMPage page;
	page.rawName = "Page (1/4)";
	page.index = 3;
	MCMControl control;
	control.type = MCMControlType::kMenu;
	control.rawLabel = "$RAW_LABEL";
	control.label = "Translated label";
	control.identity.optionIndex = 12;
	control.menu.emplace();
	control.menu->options = { "$FIRST", "$SECOND" };
	std::uint64_t token{};
	std::uint32_t calls{};
	REQUIRE(events.Subscribe([](void* a_user, const MCMHostEvent* a_event) {
		++*static_cast<std::uint32_t*>(a_user);
		CHECK(a_event->type == MCM_HOST_USER_CHANGE);
		CHECK(std::string(a_event->mod_name) == "Original name");
		CHECK(std::string(a_event->page_name) == "Page (1/4)");
		CHECK(std::string(a_event->label) == "$RAW_LABEL");
		CHECK(std::string(a_event->value_text) == "$SECOND");
		CHECK(a_event->value.integer == 1);
		CHECK(a_event->page_scoped_state == 1);
		CHECK(a_event->option_index == 12);
	},
				&calls, token) == MCM_HOST_OK);
	events.Changed(7, mod, page, control, std::int32_t{ 1 });
	CHECK(calls == 1);
}

TEST_CASE("Native host rejects null observers and isolates throwing receivers")
{
	HostEvents    events;
	std::uint64_t token{ 100 };
	CHECK(events.Subscribe(nullptr, nullptr, token) == MCM_HOST_INVALID_ARGUMENT);
	CHECK(token == 0);
	REQUIRE(events.Subscribe([](void*, const MCMHostEvent*) { throw 1; }, nullptr, token) == MCM_HOST_OK);
	std::uint32_t calls{};
	REQUIRE(events.Subscribe([](void* a_user, const MCMHostEvent*) { ++*static_cast<std::uint32_t*>(a_user); }, &calls, token) == MCM_HOST_OK);
	events.Session(MCM_HOST_SESSION_BEGIN, 9);
	CHECK(calls == 1);
}

TEST_CASE("Native host edit events retain pre-callback identity and exclude restore")
{
	HostEvents events;
	auto       snapshot = std::make_shared<MCMSnapshot>();
	MCMMod     mod;
	mod.interopID = "Script::Original";
	MCMPage page;
	page.rawName = "Before";
	MCMControl control;
	control.type = MCMControlType::kToggle;
	control.identity.stableID = "setting";
	page.controls.push_back(control);
	mod.pages.push_back(page);
	snapshot->mods.push_back(mod);
	WriteCommand command;
	command.settingID = "setting";
	command.recordingSnapshot = snapshot;
	command.recordingID = 17;
	std::vector<std::uint32_t> types;
	std::uint64_t              token{};
	REQUIRE(events.Subscribe([](void* a_user, const MCMHostEvent* a_event) {
		static_cast<std::vector<std::uint32_t>*>(a_user)->push_back(a_event->type);
		CHECK(a_event->change_id == 17);
		CHECK(a_event->session == 9);
		CHECK(std::string(a_event->page_name) == "Before");
		CHECK(a_event->value.integer == (a_event->type == MCM_HOST_USER_CHANGE ? 1 : 0));
		CHECK(a_event->confirmation_accepted == (a_event->type == MCM_HOST_USER_CHANGE ? 1U : 0U));
	},
				&types, token) == MCM_HOST_OK);
	events.Write(MCM_HOST_USER_CHANGING, 9, command, false);
	events.Write(MCM_HOST_USER_CHANGE, 9, command, true, true);
	events.Write(MCM_HOST_USER_REJECTED, 9, command, false);
	command.recordingID = 0;
	events.Write(MCM_HOST_USER_CHANGE, 9, command, true);
	CHECK(types == std::vector<std::uint32_t>{ MCM_HOST_USER_CHANGING, MCM_HOST_USER_CHANGE, MCM_HOST_USER_REJECTED });
}
