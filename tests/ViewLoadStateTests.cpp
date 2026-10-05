#include "MCMBridge/Core/HostedPageRoute.h"
#include "MCMBridge/Core/QuickOpenPage.h"
#include "MCMBridge/Core/ViewLoadState.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Navigation recovery routes the old rendered entry only for the current selection")
{
	MCMBridge::HostedPageRoute route;
	MCMBridge::ViewLoadState   view;
	const auto                 first = view.Request("mod", "old index");
	REQUIRE(route.Remap(view, first, "mod", "old index", "new index"));
	for (int frame = 0; frame < 100; ++frame)
		CHECK(route.Resolve("mod", "old index") == "new index");
	CHECK_FALSE(view.Ready("mod", "new index"));
	CHECK_FALSE(view.Complete(first));
	const auto next = view.Request("mod", "user selection");
	CHECK_FALSE(route.Remap(view, first, "mod", "old index", "stale target"));
	CHECK(route.Resolve("mod", "user selection") == "user selection");
	REQUIRE(view.Complete(next));
	CHECK(view.Ready("mod", "user selection"));
	view.Invalidate();
	CHECK_FALSE(route.Remap(view, next, "mod", "user selection", "previous save"));
}

TEST_CASE("Script page remapping invalidates its original request before target activation")
{
	MCMBridge::ViewLoadState state;
	const auto               original = state.Request("mod", "first");
	REQUIRE(state.Complete(original));
	REQUIRE(state.Remap(original, "second"));
	CHECK_FALSE(state.Complete(original));
	CHECK_FALSE(state.Remap(original, "third"));
	CHECK_FALSE(state.Ready("mod", "second"));
	const auto target = state.RevisionFor("mod", "second");
	REQUIRE(target);
	REQUIRE(state.Complete(*target));
	CHECK(state.Ready("mod", "second"));
	state.Request("other", "first");
	CHECK_FALSE(state.Remap(*target, "second"));
	state.Invalidate();
	CHECK_FALSE(state.Remap(*target, "second"));
}

TEST_CASE("Quick open resolves raw page identity without localized label or index guesses")
{
	MCMBridge::MCMMod mod;
	CHECK_FALSE(MCMBridge::ResolveQuickOpenPage(mod, ""));
	mod.pages.push_back({ .stableID = "first", .rawName = "$General", .displayName = "General" });
	mod.pages.push_back({ .stableID = "second", .rawName = "Hall (1/4)", .displayName = "General" });
	CHECK(MCMBridge::ResolveQuickOpenPage(mod, "") == "first");
	CHECK(MCMBridge::ResolveQuickOpenPage(mod, "Hall (1/4)") == "second");
	CHECK_FALSE(MCMBridge::ResolveQuickOpenPage(mod, "General"));
	CHECK_FALSE(MCMBridge::ResolveQuickOpenPage(mod, "Missing"));
	std::swap(mod.pages[0], mod.pages[1]);
	CHECK(MCMBridge::ResolveQuickOpenPage(mod, "$General") == "first");
	mod.pages.push_back({ .stableID = "duplicate", .rawName = "$General" });
	CHECK_FALSE(MCMBridge::ResolveQuickOpenPage(mod, "$General"));
	mod.pages.push_back({ .stableID = "landing", .rawName = "" });
	CHECK(MCMBridge::ResolveQuickOpenPage(mod, "") == "landing");
	mod.pages.push_back({ .stableID = "duplicate-empty", .rawName = "" });
	CHECK_FALSE(MCMBridge::ResolveQuickOpenPage(mod, ""));
}

TEST_CASE("Script closure prevents render frames reopening the previous page")
{
	MCMBridge::HostedPageRoute route;
	route.Redirect("mod", "one", "two");
	route.Close("mod", "two");
	for (int frame = 0; frame < 100; ++frame)
		CHECK(route.Resolve("mod", "one").empty());
	CHECK(route.Resolve("mod", "three") == "three");
	CHECK(route.Resolve("mod", "one") == "one");
	route.Close("mod", "one");
	CHECK(route.Resolve("other", "one") == "one");
	route.Close("mod", "one");
	route.Clear();
	CHECK(route.Resolve("mod", "one") == "one");
}

TEST_CASE("Script page routes survive rendering but yield to explicit navigation and session reset")
{
	MCMBridge::HostedPageRoute route;
	CHECK(route.Resolve("mod", "one") == "one");
	route.Redirect("mod", "one", "two");
	CHECK(route.Resolve("mod", "one") == "two");
	CHECK(route.Resolve("mod", "one") == "two");
	route.Redirect("mod", "two", "three");
	CHECK(route.Resolve("mod", "one") == "three");
	CHECK(route.Resolve("mod", "three") == "three");
	CHECK(route.Resolve("mod", "four") == "four");
	CHECK(route.Resolve("mod", "one") == "one");
	route.Redirect("mod", "one", "two");
	CHECK(route.Resolve("other", "one") == "one");
	CHECK(route.Resolve("mod", "one") == "one");
	route.Redirect("mod", "one", "two");
	route.Clear();
	CHECK(route.Resolve("mod", "one") == "one");
}

TEST_CASE("Old page operations cannot recover navigation for a newer selection")
{
	MCMBridge::ViewLoadState state;
	const auto               revision = state.Request("mod", "one");
	REQUIRE(state.RevisionFor("mod", "one") == revision);
	state.Request("mod", "two");
	CHECK_FALSE(state.RevisionFor("mod", "one"));
	CHECK_FALSE(state.Complete(revision, "stale error"));
	CHECK_FALSE(state.Remap(revision, "old renamed page"));
	state.Invalidate();
	CHECK_FALSE(state.Current(revision));
}

TEST_CASE("A view stays unavailable until its current activation completes")
{
	MCMBridge::ViewLoadState state;
	const auto               request = state.Request("mod", "page");
	REQUIRE_FALSE(state.Ready("mod", "page"));
	REQUIRE(state.Complete(request));
	REQUIRE(state.Ready("mod", "page"));
	REQUIRE_FALSE(state.Ready("mod", "other"));
	REQUIRE(state.Request("mod", "page") == request);
}

TEST_CASE("Returning to a page does not accept its previous activation")
{
	MCMBridge::ViewLoadState state;
	const auto               old = state.Request("mod", "a");
	state.Request("mod", "b");
	const auto current = state.Request("mod", "a");
	REQUIRE_FALSE(state.Complete(old));
	REQUIRE_FALSE(state.Ready("mod", "a"));
	REQUIRE(state.Complete(current));
}

TEST_CASE("Session invalidation rejects late success and late failure")
{
	MCMBridge::ViewLoadState state;
	const auto               old = state.Request("mod", "a");
	state.Invalidate();
	REQUIRE_FALSE(state.Current(old));
	REQUIRE_FALSE(state.Complete(old, "old failure"));
	REQUIRE(state.Error().empty());
	REQUIRE_FALSE(state.Ready("mod", "a"));
}

TEST_CASE("An activation failure remains distinct from an empty ready page")
{
	MCMBridge::ViewLoadState state;
	const auto               request = state.Request("mod", "empty");
	REQUIRE(state.Complete(request, "Unavailable"));
	REQUIRE_FALSE(state.Ready("mod", "empty"));
	REQUIRE(state.Error() == "Unavailable");
	state.Invalidate();
	REQUIRE(state.Error().empty());
	REQUIRE(state.Complete(state.Request("mod", "empty")));
	REQUIRE(state.Ready("mod", "empty"));
}
