#include "MCMBridge/Core/HostControlCapture.h"
#include "MCMBridge/Core/HostPageState.h"

#include <catch2/catch_test_macros.hpp>

using namespace MCMBridge;

namespace
{
	ClassicPageBuffers Page()
	{
		return { { 3 }, { "Toggle" }, { "" }, { 0 }, { "ToggleState" } };
	}
}

TEST_CASE("Native host captures cursor writes and flags in order", "[host]")
{
	HostControlCapture capture;
	REQUIRE(capture.Observe(HostProtocolCallKind::kSet, "optionCursorIndex", std::int32_t{ 4 }));
	REQUIRE(capture.Observe(HostProtocolCallKind::kSet, "optionCursor.numValue", 1.0F));
	REQUIRE(capture.Observe(HostProtocolCallKind::kSet, "optionCursor.strValue", std::string("{0}")));
	REQUIRE(capture.Observe(HostProtocolCallKind::kInvoke, "setOptionFlags", std::vector<std::int32_t>{ 2, 0 }));
	REQUIRE(capture.Observe(HostProtocolCallKind::kInvoke, "invalidateOptionData", false));
	const auto result = capture.Complete();
	REQUIRE(result.controls.size() == 3);
	CHECK(result.controls[0].index == 4);
	CHECK(result.controls[0].number == 1.0F);
	CHECK(result.controls[1].text == "{0}");
	CHECK(result.controls[2].index == 2);
	CHECK(result.controls[2].flags == 0);
	CHECK(result.redrawRequested);
	CHECK_FALSE(result.resetRequested);
	CHECK_FALSE(result.malformed);
	CHECK(capture.Complete().controls.empty());
}

TEST_CASE("Native host refuses values without a valid cursor and distinguishes invocation", "[host]")
{
	HostControlCapture capture;
	CHECK_FALSE(capture.Observe(HostProtocolCallKind::kInvoke, "optionCursorIndex", std::int32_t{ 2 }));
	capture.Observe(HostProtocolCallKind::kSet, "optionCursor.numValue", 1.0F);
	CHECK(capture.Complete().malformed);
	capture.Observe(HostProtocolCallKind::kSet, "optionCursorIndex", std::int32_t{ 2 });
	capture.Complete();
	capture.Observe(HostProtocolCallKind::kSet, "optionCursor.numValue", 1.0F);
	CHECK(capture.Complete().malformed);
}

TEST_CASE("Native host page flush supersedes earlier control deltas", "[host]")
{
	HostControlCapture capture;
	capture.Observe(HostProtocolCallKind::kInvoke, "setOptionFlags", std::vector<std::int32_t>{ 2, 1 });
	capture.Observe(HostProtocolCallKind::kInvoke, "flushOptionBuffers", std::int32_t{ 3 });
	capture.Observe(HostProtocolCallKind::kInvoke, "setOptionFlags", std::vector<std::int32_t>{ 1, 0 });
	const auto result = capture.Complete();
	REQUIRE(result.controls.size() == 1);
	CHECK(result.controls[0].index == 1);
}

TEST_CASE("Native host retains navigation and dynamic dialog data", "[host]")
{
	HostControlCapture capture;
	capture.Observe(HostProtocolCallKind::kInvoke, "setPageNames", std::vector<std::string>{ "A/B", "" });
	capture.Observe(HostProtocolCallKind::kInvoke, "setMenuDialogOptions", std::vector<std::string>{ "One", "Two" });
	capture.Observe(HostProtocolCallKind::kInvoke, "setSliderDialogParams", std::vector<float>{ 2, 1, 0, 10, 1 });
	capture.Observe(HostProtocolCallKind::kInvoke, "forcePageReset", false);
	capture.Observe(HostProtocolCallKind::kInvoke, "setInputDialogParams", std::string("Existing text"));
	const auto result = capture.Complete();
	REQUIRE(result.navigation);
	CHECK(result.navigation->at(0) == "A/B");
	REQUIRE(result.menuOptions);
	CHECK(result.menuOptions->size() == 2);
	REQUIRE(result.sliderParameters);
	CHECK(result.sliderParameters->at(0) == 2);
	CHECK(result.resetRequested);
	CHECK(result.inputText == "Existing text");
}

TEST_CASE("Host state preserves UI-only changes when script buffers remain unchanged", "[host]")
{
	HostPageState state;
	state.Reconcile("General", 0, Page());
	const auto         structure = state.StructureRevision();
	HostControlChanges changes;
	changes.controls.push_back({ .index = 0, .flags = 1, .number = 1.0F });
	REQUIRE(state.Apply(changes));
	state.Reconcile("General", 0, Page());
	const auto* page = state.Get("General", 0);
	REQUIRE(page);
	CHECK(page->numericValues[0] == 1);
	CHECK(page->optionFlags[0] == 259);
	CHECK(state.StructureRevision() == structure);
	CHECK_FALSE(state.Get("Other", 0));
}

TEST_CASE("Host state reconciles direct changes and authoritative page rebuilds", "[host]")
{
	HostPageState state;
	state.Reconcile("General", 0, Page());
	HostControlChanges changes;
	changes.controls.push_back({ .index = 0, .number = 1.0F });
	REQUIRE(state.Apply(changes));
	auto changed = Page();
	changed.numericValues[0] = 2;
	state.Reconcile("General", 0, changed);
	REQUIRE(state.Get("General", 0));
	CHECK(state.Get("General", 0)->numericValues[0] == 2);
	state.Invalidate();
	CHECK_FALSE(state.Get("General", 0));
	state.Reconcile("General", 0, Page());
	CHECK(state.Get("General", 0)->numericValues[0] == 0);
}

TEST_CASE("Host state rejects malformed deltas and observes reset barriers", "[host]")
{
	HostPageState state;
	state.Reconcile("General", 0, Page());
	HostControlChanges changes;
	SECTION("Invalid slot") { changes.controls.push_back({ .index = 1, .number = 1.0F }); }
	SECTION("Reset") { changes.resetRequested = true; }
	SECTION("Malformed") { changes.malformed = true; }
	CHECK_FALSE(state.Apply(changes));
	CHECK_FALSE(state.Get("General", 0));
}
