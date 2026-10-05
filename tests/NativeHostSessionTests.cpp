#include "MCMBridge/Core/CustomContentPolicy.h"
#include "MCMBridge/Core/NativeHostSession.h"

#include <catch2/catch_test_macros.hpp>

using namespace MCMBridge;

TEST_CASE("Changing header presentation preserves identity coverage and publishes the new appearance")
{
	NativeHostSession host;
	host.Reset(1);
	const auto token = host.Open(1, "mod");
	REQUIRE(token);
	const auto revision = host.IdentityRevision();
	for (int iteration = 0; iteration < 2233; ++iteration) {
		REQUIRE(host.BeginPage(*token, "General", 0));
		const auto label = iteration % 2 ? "<font color='#6699ff'>Header" : "<font color='#ff3389'>Header";
		REQUIRE(host.AddOption(*token, 1, label, "", 0, 0, "") == 256);
		REQUIRE(host.AddOption(*token, 3, "Enabled", "", 1, 0, "Enabled") == 257);
		REQUIRE(host.Publish(*token));
		CHECK(host.IdentityRevision() == revision);
		CHECK(host.Read()->buffers.labels[0] == label);
		CHECK(host.Read()->valueRevision == static_cast<std::uint64_t>(iteration + 1));
	}
	SECTION("changing header into a setting invalidates the index")
	{
		REQUIRE(host.BeginPage(*token, "General", 0));
		REQUIRE(host.AddOption(*token, 3, "Header", "", 0, 0, "") == 256);
		REQUIRE(host.Publish(*token));
	}
	SECTION("changing a setting label still invalidates the index")
	{
		auto buffers = host.Read()->buffers;
		buffers.labels[1] = "Different setting";
		REQUIRE(host.ImportBuffers(*token, std::move(buffers)));
		REQUIRE(host.Publish(*token));
	}
	CHECK(host.IdentityRevision() > revision);
}

TEST_CASE("Full identity recapture replaces historical pages without masking new invalidation")
{
	NativeHostSession host;
	host.Reset(1);
	const auto token = host.Open(1, "mod");
	REQUIRE(token);
	for (int index = 0; index < 4; ++index) {
		REQUIRE(host.BeginPage(*token, std::to_string(index), index));
		REQUIRE(host.AddOption(*token, 3, "Old", "", 0, 1, "State") >= 0);
		REQUIRE(host.Publish(*token));
	}
	host.BeginIdentityCapture(*token);
	const auto revision = host.IdentityRevision();
	for (int index = 0; index < 3; ++index) {
		REQUIRE(host.BeginPage(*token, std::to_string(index), index));
		REQUIRE(host.AddOption(*token, 3, "New", "", 1, 0, "State") >= 0);
		REQUIRE(host.Publish(*token));
		CHECK(host.IdentityRevision() == revision);
	}
	SECTION("a changed page captured in this pass invalidates coverage")
	{
		REQUIRE(host.BeginPage(*token, "0", 0));
		REQUIRE(host.AddOption(*token, 3, "Changed again", "", 1, 0, "Other") >= 0);
		REQUIRE(host.Publish(*token));
	}
	SECTION("a reused current page remains tracked")
	{
		REQUIRE(host.BeginPage(*token, "3", 3));
		REQUIRE(host.Publish(*token));
	}
	SECTION("explicit reset remains authoritative") { REQUIRE(host.SetPresentation(*token, 4, "")); }
	SECTION("navigation remains authoritative") { REQUIRE(host.SetNavigation(*token, { "New" })); }
	CHECK(host.IdentityRevision() > revision);
}

TEST_CASE("Identity recapture rejects expired ownership without dropping active evidence")
{
	NativeHostSession host;
	host.Reset(1);
	const auto token = host.Open(1, "mod");
	REQUIRE(token);
	REQUIRE(host.BeginPage(*token, "Page", 0));
	REQUIRE(host.AddOption(*token, 3, "Old", "", 0, 0, "") >= 0);
	REQUIRE(host.Publish(*token));
	host.BeginIdentityCapture(*token + 1);
	const auto revision = host.IdentityRevision();
	REQUIRE(host.BeginPage(*token, "Page", 0));
	host.BeginIdentityCapture(*token);
	REQUIRE(host.Publish(*token));
	CHECK(host.IdentityRevision() > revision);
}

TEST_CASE("Native identity coverage survives ordinary page traversal but not explicit reset")
{
	NativeHostSession host;
	host.Reset(1);
	const auto token = host.Open(1, "mod");
	REQUIRE(token);
	const auto revision = host.IdentityRevision();
	REQUIRE(host.BeginPage(*token, "First", 0));
	REQUIRE(host.Publish(*token));
	const auto valueRevision = host.Read()->valueRevision;
	REQUIRE(host.BeginPage(*token, "Second", 1));
	REQUIRE(host.Publish(*token));
	CHECK(host.Read()->valueRevision > valueRevision);
	CHECK(host.IdentityRevision() == revision);
	REQUIRE(host.BeginPage(*token, "First", 0));
	REQUIRE(host.Publish(*token));
	CHECK(host.IdentityRevision() == revision);
	REQUIRE(host.SetPresentation(*token, 4, ""));
	CHECK(host.IdentityRevision() > revision);
	const auto resetRevision = host.IdentityRevision();
	REQUIRE(host.SetNavigation(*token, { "First", "New" }));
	CHECK(host.IdentityRevision() > resetRevision);
}

TEST_CASE("Native identity coverage detects changed controls and out of build updates")
{
	NativeHostSession host;
	host.Reset(1);
	const auto token = host.Open(1, "mod");
	REQUIRE(token);
	REQUIRE(host.BeginPage(*token, "First", 0));
	REQUIRE(host.AddOption(*token, 3, "Toggle", "", 0, 1, "State") == 256);
	REQUIRE(host.Publish(*token));
	const auto original = host.IdentityRevision();
	REQUIRE(host.BeginPage(*token, "Second", 1));
	REQUIRE(host.Publish(*token));
	REQUIRE(host.BeginPage(*token, "First", 0));
	REQUIRE(host.AddOption(*token, 3, "Renamed", "", 0, 1, "State") == 256);
	REQUIRE(host.Publish(*token));
	CHECK(host.IdentityRevision() > original);
	const auto renamed = host.IdentityRevision();
	REQUIRE(host.SetValue(*token, 256, "", 1));
	REQUIRE(host.Publish(*token));
	CHECK(host.IdentityRevision() > renamed);
}

TEST_CASE("Native external view requests cannot borrow historical or replaced owner identities")
{
	NativeHostSession host;
	host.Reset(1);
	const auto first = host.Open(1, "first", 101);
	REQUIRE(first);
	REQUIRE(host.ReadIdentity(*first));
	CHECK(host.ReadIdentity(*first)->modID == "first");
	REQUIRE(host.BeginPage(*first, "Page", 0));
	REQUIRE(host.Publish(*first));
	REQUIRE(host.Close(*first));
	REQUIRE(host.Read());
	CHECK_FALSE(host.ReadIdentity(*first));
	const auto second = host.Open(1, "second", 102);
	REQUIRE(second);
	CHECK(host.Read()->modID == "first");
	CHECK(host.ReadIdentity(*second)->modID == "second");
	CHECK(host.TokenForOwner(101) == 0);
	CHECK_FALSE(host.ReadIdentity(host.TokenForOwner(101)));
	host.Reset(2);
	CHECK_FALSE(host.ReadIdentity(*second));
}

TEST_CASE("Native close requests coalesce without closing the execution owner")
{
	NativeHostSession host;
	host.Reset(1);
	const auto token = host.Open(1, "mod");
	REQUIRE(token);
	REQUIRE(host.RequestPage(*token, "Page", 0));
	CHECK_FALSE(host.RequestClose(*token + 1, true));
	REQUIRE(host.RequestClose(*token, false));
	CHECK_FALSE(host.TakePageRequest(*token));
	CHECK_FALSE(host.RequestPage(*token, "Other", 1));
	CHECK_FALSE(host.TakeCloseRequest(*token + 1));
	REQUIRE(host.TakeCloseRequest(*token) == false);
	CHECK(host.IsActive(*token));
	CHECK_FALSE(host.TakeCloseRequest(*token));
	for (int index = 0; index < 2233; ++index)
		REQUIRE(host.RequestClose(*token, index == 1));
	REQUIRE(host.TakeCloseRequest(*token) == true);
	REQUIRE(host.RequestClose(*token, true));
	REQUIRE(host.Close(*token));
	const auto next = host.Open(1, "mod");
	REQUIRE(next);
	CHECK_FALSE(host.TakeCloseRequest(*token));
	CHECK_FALSE(host.TakeCloseRequest(*next));
	REQUIRE(host.RequestClose(*next, true));
	host.Reset(2);
	CHECK_FALSE(host.TakeCloseRequest(*next));
}

TEST_CASE("Native script page requests coalesce and cannot outlive their execution owner")
{
	NativeHostSession host;
	host.Reset(1);
	const auto token = host.Open(1, "mod");
	REQUIRE(token);
	CHECK_FALSE(host.RequestPage(0, "General", 0));
	CHECK_FALSE(host.RequestPage(*token, "General", -1));
	REQUIRE(host.RequestPage(*token, "General", 0));
	REQUIRE(host.RequestPage(*token, "Hall (1/4)", 2));
	CHECK_FALSE(host.RequestPage(*token + 1, "Foreign", 3));
	CHECK_FALSE(host.TakePageRequest(*token + 1));
	const auto request = host.TakePageRequest(*token);
	REQUIRE(request);
	CHECK(request->name == "Hall (1/4)");
	CHECK(request->index == 2);
	CHECK_FALSE(host.TakePageRequest(*token));
	REQUIRE(host.RequestPage(*token, "", 1));
	REQUIRE(host.TakePageRequest(*token));
	for (std::int32_t index = 0; index < 2233; ++index)
		REQUIRE(host.RequestPage(*token, "Page", index));
	CHECK(host.TakePageRequest(*token)->index == 2232);
	REQUIRE(host.RequestPage(*token, "Before close", 0));
	REQUIRE(host.Close(*token));
	const auto next = host.Open(1, "mod");
	REQUIRE(next);
	CHECK_FALSE(host.TakePageRequest(*token));
	CHECK_FALSE(host.TakePageRequest(*next));
	REQUIRE(host.RequestPage(*next, "Before load", 0));
	host.Reset(2);
	CHECK_FALSE(host.TakePageRequest(*next));
}

TEST_CASE("Native navigation overrides retain raw names and expire at callback boundaries")
{
	NativeHostSession host;
	host.Reset(1);
	const auto token = host.Open(1, "mod");
	REQUIRE(token);
	CHECK_FALSE(host.ReadNavigation(*token));
	REQUIRE(host.BeginPage(*token, "General", 0));
	REQUIRE(host.Publish(*token));
	const auto                     before = host.Read();
	const std::vector<std::string> pages{ "Hall (1/4)", "$Raw", "", "General", "General" };
	REQUIRE(host.SetNavigation(*token, pages));
	CHECK(host.ReadNavigation(*token) == pages);
	CHECK(host.Read() == before);
	REQUIRE(host.Publish(*token));
	CHECK(host.Read()->resetRevision == before->resetRevision + 1);
	const auto changed = host.Read();
	REQUIRE(host.SetNavigation(*token, pages));
	REQUIRE(host.Publish(*token));
	CHECK(host.Read() == changed);
	CHECK_FALSE(host.SetNavigation(*token + 1, {}));
	host.ClearNavigationOverride(*token + 1);
	CHECK(host.ReadNavigation(*token) == pages);
	host.ClearNavigationOverride(*token);
	CHECK_FALSE(host.ReadNavigation(*token));
	REQUIRE(host.SetNavigation(*token, pages));
	REQUIRE(host.Publish(*token));
	CHECK(host.Read() == changed);
	REQUIRE(host.SetNavigation(*token, {}));
	REQUIRE(host.ReadNavigation(*token));
	CHECK(host.ReadNavigation(*token)->empty());
	REQUIRE(host.Close(*token));
	CHECK_FALSE(host.ReadNavigation(*token));
	const auto next = host.Open(1, "other");
	REQUIRE(next);
	CHECK_FALSE(host.SetNavigation(*token, pages));
	CHECK_FALSE(host.ReadNavigation(*next));
	REQUIRE(host.SetNavigation(*next, pages));
	host.Reset(2);
	CHECK_FALSE(host.ReadNavigation(*next));
}

TEST_CASE("Native option cursor reads live values without publishing or crossing owners")
{
	NativeHostSession host;
	host.Reset(1);
	const auto token = host.Open(1, "mod", 17);
	REQUIRE(token);
	CHECK_FALSE(host.ReadOptionCursor(*token));
	REQUIRE(host.BeginPage(*token, "Keys", 0));
	REQUIRE(host.AddOption(*token, 7, "Key", "", 42, 1, "key___1") == 256);
	REQUIRE(host.Publish(*token));
	const auto published = host.Read();
	REQUIRE(host.SetOptionCursor(*token, 0));
	REQUIRE(host.ReadOptionCursor(*token));
	CHECK(host.ReadOptionCursor(*token)->type == 7);
	CHECK(host.ReadOptionCursor(*token)->value == 42);
	REQUIRE(host.SetValue(*token, 256, "", 54));
	CHECK(host.ReadOptionCursor(*token)->value == 54);
	CHECK(host.Read() == published);
	CHECK_FALSE(host.SetOptionCursor(*token + 1, 1));
	CHECK_FALSE(host.ReadOptionCursor(*token + 1));
	CHECK(host.ReadOptionCursor(*token)->value == 54);
	CHECK_FALSE(host.SetOptionCursor(*token, 256));
	CHECK_FALSE(host.ReadOptionCursor(*token));
	REQUIRE(host.SetOptionCursor(*token, 0));
	REQUIRE(host.BeginPage(*token, "Other", 1));
	CHECK_FALSE(host.ReadOptionCursor(*token));
	REQUIRE(host.Close(*token));
	CHECK_FALSE(host.ReadOptionCursor(*token));
	const auto next = host.Open(1, "mod", 17);
	REQUIRE(next);
	REQUIRE(host.BeginPage(*next, "Keys", 0));
	CHECK_FALSE(host.SetOptionCursor(*token, 0));
	host.Reset(2);
	CHECK_FALSE(host.ReadOptionCursor(*next));
}

TEST_CASE("Native Helper partial updates preserve the other control fields and publish together")
{
	NativeHostSession host;
	host.Reset(1);
	const auto token = host.Open(1, "mod", 17);
	REQUIRE(token);
	REQUIRE(host.BeginPage(*token, "General", 0));
	REQUIRE(host.AddOption(*token, 4, "Slider", "{1}", 2, 1, "slider") == 256);
	CHECK_FALSE(host.UpdateControl(*token, 0, "{0}", 3.0F, 0));
	REQUIRE(host.Publish(*token));
	const auto previous = host.Read();
	REQUIRE(host.UpdateControl(*token, 0, {}, 3.0F, {}));
	REQUIRE(host.UpdateControl(*token, 0, "{0}", {}, {}));
	REQUIRE(host.UpdateControl(*token, 0, {}, {}, 0));
	CHECK(host.Read() == previous);
	REQUIRE(host.Publish(*token));
	const auto current = host.Read();
	CHECK(current->buffers.numericValues[0] == 3);
	CHECK(current->buffers.stringValues[0] == "{0}");
	CHECK(current->buffers.optionFlags[0] == 4);
	CHECK(current->structureRevision == previous->structureRevision);
	CHECK(current->valueRevision == previous->valueRevision + 1);
	CHECK(current->buffers.labels[0] == "Slider");
	CHECK(current->buffers.stateNames[0] == "slider");
	// The adapter mirrors only these host fields before the final callback import.
	auto mirror = previous->buffers;
	mirror.numericValues[0] = 3;
	mirror.stringValues[0] = "{0}";
	mirror.optionFlags[0] = 4;
	REQUIRE(host.ImportBuffers(*token, std::move(mirror)));
	REQUIRE(host.Publish(*token));
	CHECK(host.Read() == current);
	REQUIRE(host.SetValue(*token, 256, "{2}", 4));
	REQUIRE(host.UpdateControl(*token, 0, {}, 5.0F, {}));
	REQUIRE(host.Publish(*token));
	CHECK(host.Read()->buffers.numericValues[0] == 5);
	CHECK(host.Read()->buffers.stringValues[0] == "{2}");
}

TEST_CASE("Native Helper updates reject missing slots and stale owners without partial effects")
{
	NativeHostSession host;
	host.Reset(1);
	const auto token = host.Open(1, "mod", 17);
	REQUIRE(token);
	CHECK_FALSE(host.UpdateControl(*token, 0, "text", 2.0F, 0));
	REQUIRE(host.BeginPage(*token, "General", 0));
	REQUIRE(host.AddOption(*token, 3, "Toggle", "", 0, 1, "") == 256);
	REQUIRE(host.Publish(*token));
	const auto previous = host.Read();
	CHECK_FALSE(host.UpdateControl(*token, -1, "text", 2.0F, 0));
	CHECK_FALSE(host.UpdateControl(*token, 128, "text", 2.0F, 0));
	CHECK_FALSE(host.UpdateControl(*token, 256, "text", 2.0F, 0));
	CHECK_FALSE(host.UpdateControl(*token, 1, "text", 2.0F, 0));
	CHECK_FALSE(host.UpdateControl(*token, 0, "text", 2.0F, -1));
	CHECK_FALSE(host.UpdateControl(*token, 0, "text", 2.0F, 0x800000));
	CHECK_FALSE(host.UpdateControl(*token + 1, 0, "text", 2.0F, 0));
	CHECK_FALSE(host.UpdateControl(*token, 0, "text", 2.0F, 0, 4));
	REQUIRE(host.Publish(*token));
	CHECK(host.Read() == previous);
	REQUIRE(host.Close(*token));
	CHECK_FALSE(host.UpdateControl(*token, 0, "text", 2.0F, 0));
	host.Reset(2);
	CHECK_FALSE(host.UpdateControl(*token, 0, "text", 2.0F, 0));
}

TEST_CASE("Native message responses are scoped and consumed exactly once")
{
	NativeHostSession host;
	host.Reset(1);
	const auto token = host.Open(1, "mod");
	REQUIRE(token);
	CHECK(host.BeginMessage(0) == 0);
	const auto request = host.BeginMessage(*token);
	REQUIRE(request > 0);
	CHECK(host.BeginMessage(*token) == 0);
	CHECK(host.TakeMessage(*token, request) == -1);
	CHECK(host.IsMessageActive(*token, request));
	CHECK_FALSE(host.CompleteMessage(*token, request + 1, true));
	CHECK_FALSE(host.CompleteMessage(*token + 1, request, true));
	REQUIRE(host.CompleteMessage(*token, request, true));
	CHECK_FALSE(host.IsMessageActive(*token, request));
	CHECK_FALSE(host.CompleteMessage(*token, request, false));
	CHECK(host.TakeMessage(*token, request) == 1);
	CHECK(host.TakeMessage(*token, request) == -2);
	const auto next = host.BeginMessage(*token);
	REQUIRE(next > request);
	CHECK_FALSE(host.CompleteMessage(*token, request, true));
	REQUIRE(host.CompleteMessage(*token, next, false));
	CHECK(host.TakeMessage(*token, next) == 0);
}

TEST_CASE("Native message timeout and save invalidation reject late accept clicks")
{
	NativeHostSession host;
	host.Reset(1);
	const auto token = host.Open(1, "mod");
	REQUIRE(token);
	const auto request = host.BeginMessage(*token);
	REQUIRE(request > 0);
	REQUIRE(host.Close(*token));
	CHECK_FALSE(host.IsMessageActive(*token, request));
	CHECK(host.TakeMessage(*token, request) == -2);
	const auto nextToken = host.Open(1, "other");
	REQUIRE(nextToken);
	const auto nextRequest = host.BeginMessage(*nextToken);
	REQUIRE(nextRequest > request);
	CHECK_FALSE(host.CompleteMessage(*token, request, true));
	CHECK_FALSE(host.CompleteMessage(*nextToken, request, true));
	CHECK(host.TakeMessage(*nextToken, nextRequest) == -1);
	host.Reset(2);
	CHECK_FALSE(host.CompleteMessage(*nextToken, nextRequest, true));
	CHECK(host.TakeMessage(*nextToken, nextRequest) == -2);
}

TEST_CASE("Custom content logs count page visits rather than render frames")
{
	CustomContentVisits visits;
	REQUIRE(visits.Observe("mod", "page"));
	for (int frame = 0; frame < 500; ++frame)
		CHECK_FALSE(visits.Observe("mod", "page"));
	CHECK(visits.count == 1);
	visits.Leave();
	CHECK(visits.Observe("mod", "page"));
	CHECK(visits.count == 2);
	CHECK(visits.Observe("other", "page"));
	CHECK(visits.count == 3);
}

TEST_CASE("Background custom content results do not split or create user visits")
{
	CustomContentVisits visits;
	REQUIRE(visits.Observe("mod", "page"));
	CHECK(visits.Observe("other", "automatic", CustomContentOrigin::kScan));
	CHECK(visits.Observe("mod", "page", CustomContentOrigin::kRestore));
	CHECK_FALSE(visits.Observe("mod", "page"));
	CHECK(visits.count == 1);
	CHECK(visits.scanResults == 1);
	CHECK(visits.restoreResults == 1);
	visits.Leave();
	CHECK(visits.Observe("other", "automatic", CustomContentOrigin::kScan));
	CHECK(visits.count == 1);
	CHECK(visits.Observe("mod", "page"));
	CHECK(visits.count == 2);
	visits = {};
	CHECK(visits.count == 0);
	CHECK(visits.scanResults == 0);
	CHECK(visits.restoreResults == 0);
	CHECK(visits.Observe("mod", "page"));
}

TEST_CASE("Native custom content preserves offsets and identifies DDS without filesystem claims")
{
	CHECK(CustomContentPlaceholder("textures/Test.DdS") == "Missing DDS Source");
	CHECK(CustomContentPlaceholder("menu.swf") == "Missing SWF Source");
	NativeHostSession host;
	host.Reset(1);
	const auto token = host.Open(1, "mod");
	REQUIRE(token);
	REQUIRE(host.BeginPage(*token, "Custom", 0));
	REQUIRE(host.SetCustomContent(*token, "test.dds", 20, -10));
	REQUIRE(host.Publish(*token));
	const auto previous = host.Read();
	CHECK(previous->customX == 20);
	CHECK(previous->customY == -10);
	REQUIRE(host.SetCustomContent(*token, "test.dds", 21, -10));
	REQUIRE(host.Publish(*token));
	CHECK(host.Read()->valueRevision > previous->valueRevision);
	REQUIRE(host.BeginPage(*token, "Next", 1));
	REQUIRE(host.Publish(*token));
	CHECK(host.Read()->customSource.empty());
	CHECK(host.Read()->customX == 0);
	CHECK(host.Read()->customY == 0);
}

TEST_CASE("Native registration epochs invalidate work independently of page ownership")
{
	NativeHostSession host;
	CHECK(host.Session() == 0);
	host.Reset(12);
	const auto epoch = host.Session();
	const auto token = host.Open(epoch, "menu");
	REQUIRE(token);
	REQUIRE(host.Close(*token));
	CHECK(host.Session() == epoch);
	host.Reset(13);
	CHECK(host.Session() != epoch);
	CHECK_FALSE(host.Open(epoch, "old menu"));
	CHECK_FALSE(host.IsActive(*token));
}

TEST_CASE("Native presentation and completed dialogs stay within the bound owner")
{
	NativeHostSession host;
	host.Reset(1);
	const auto token = host.Open(1, "mod");
	REQUIRE(token);
	REQUIRE(host.BeginPage(*token, "General", 0));
	REQUIRE(host.AddOption(*token, 5, "Menu", "", 0, 0, "") == 256);
	REQUIRE(host.SetPresentation(*token, 0, "$Title"));
	REQUIRE(host.SetPresentation(*token, 2, "custom.swf"));
	REQUIRE(host.Publish(*token));
	const auto previous = host.Read();
	CHECK(previous->title == "$Title");
	CHECK(previous->customSource == "custom.swf");
	const auto request = host.BeginDialog(*token, 256);
	REQUIRE(request > 0);
	CHECK_FALSE(host.ReadDialog(*token));
	REQUIRE(host.SetDialogOptions(request, { "First", "Second" }));
	REQUIRE(host.FinishDialog(*token, request));
	REQUIRE(host.ReadDialog(*token));
	CHECK(host.ReadDialog(*token)->options.size() == 2);
	REQUIRE(host.SetPresentation(*token, 4, ""));
	REQUIRE(host.Publish(*token));
	CHECK(host.Read()->structureRevision == previous->structureRevision);
	CHECK(host.Read()->resetRevision == previous->resetRevision + 1);
	REQUIRE(host.BeginPage(*token, "Other", 1));
	CHECK_FALSE(host.ReadDialog(*token));
	REQUIRE(host.Publish(*token));
	CHECK(host.Read()->customSource.empty());
	REQUIRE(host.Close(*token));
	CHECK_FALSE(host.IsActive(*token));
	CHECK_FALSE(host.SetPresentation(*token, 0, "Late"));
}

TEST_CASE("Native dialogs preserve defaults and isolate request lifetimes")
{
	NativeHostSession host;
	host.Reset(1);
	const auto token = host.Open(1, "mod");
	REQUIRE(token);
	REQUIRE(host.BeginPage(*token, "General", 0));
	REQUIRE(host.AddOption(*token, 4, "Slider", "", 0, 0, "") == 256);
	CHECK(host.BeginDialog(*token, 256) == 0);
	REQUIRE(host.Publish(*token));
	const auto request = host.BeginDialog(*token, 256);
	REQUIRE(request > 0);
	CHECK(host.BeginDialog(*token, 256) == 0);
	CHECK_FALSE(host.SetDialogIndex(request, 5, 0, 1));
	CHECK_FALSE(host.SetSliderParameter(request, 5, 1));
	const auto defaults = host.FinishDialog(*token, request);
	REQUIRE(defaults);
	CHECK(defaults->slider == std::array<float, 5>{ 0, 0, 0, 1, 1 });
	CHECK_FALSE(host.FinishDialog(*token, request));
	const auto next = host.BeginDialog(*token, 256);
	REQUIRE(next > request);
	CHECK_FALSE(host.SetSliderParameter(request, 0, 42));
	REQUIRE(host.SetSliderParameter(next, 0, 17));
	REQUIRE(host.SetSliderParameter(next, 4, 0.25F));
	const auto changed = host.FinishDialog(*token, next);
	REQUIRE(changed);
	CHECK(changed->slider[0] == 17);
	CHECK(changed->slider[4] == 0.25F);
	CHECK(host.Read()->buffers.numericValues[0] == 0);
}

TEST_CASE("Native dialogs retain complete options and invalidate on page and session changes")
{
	NativeHostSession host;
	host.Reset(1);
	const auto token = host.Open(1, "mod");
	REQUIRE(token);
	REQUIRE(host.BeginPage(*token, "", -1));
	for (const auto type : { 5, 6, 8, 3 })
		REQUIRE(host.AddOption(*token, type, "Control", "", 0, 0, "") >= 0);
	REQUIRE(host.Publish(*token));
	CHECK(host.BeginDialog(*token, 3) == 0);
	const auto menu = host.BeginDialog(*token, 0);
	REQUIRE(menu > 0);
	REQUIRE(host.SetDialogOptions(menu, { "$One", "Two/Three", "" }));
	REQUIRE(host.SetDialogIndex(menu, 5, 0, 2));
	const auto data = host.FinishDialog(*token, menu);
	REQUIRE(data);
	CHECK(data->options == std::vector<std::string>{ "$One", "Two/Three", "" });
	CHECK(data->menu == std::array<std::int32_t, 2>{ 2, -1 });
	const auto color = host.BeginDialog(*token, 1);
	REQUIRE(color > 0);
	REQUIRE(host.SetDialogIndex(color, 6, 0, 0xAABBCC));
	const auto colors = host.FinishDialog(*token, color);
	REQUIRE(colors);
	CHECK(colors->color == std::array<std::int32_t, 2>{ 0xAABBCC, -1 });
	const auto input = host.BeginDialog(*token, 2);
	REQUIRE(input > 0);
	REQUIRE(host.SetDialogInput(input, "$Raw input"));
	const auto text = host.FinishDialog(*token, input);
	REQUIRE(text);
	CHECK(text->input == "$Raw input");
	const auto stale = host.BeginDialog(*token, 2);
	REQUIRE(stale > 0);
	REQUIRE(host.BeginPage(*token, "Other", 1));
	CHECK_FALSE(host.SetDialogInput(stale, "Late"));
	CHECK_FALSE(host.FinishDialog(*token, stale));
	host.Reset(2);
	CHECK_FALSE(host.SetDialogOptions(menu, {}));
}

TEST_CASE("Native dialog cancellation and close reject late setters across repeated owners")
{
	NativeHostSession host;
	host.Reset(1);
	std::int32_t previous = 0;
	for (int iteration = 0; iteration < 2233; ++iteration) {
		const auto token = host.Open(1, "mod");
		REQUIRE(token);
		REQUIRE(host.BeginPage(*token, "", -1));
		REQUIRE(host.AddOption(*token, 4, "Slider", "", 0, 0, "") == 0);
		REQUIRE(host.Publish(*token));
		const auto request = host.BeginDialog(*token, 0);
		REQUIRE(request > previous);
		CHECK_FALSE(host.SetSliderParameter(previous, 0, 42));
		CHECK_FALSE(host.CancelDialog(*token, previous));
		REQUIRE(host.CancelDialog(*token, request));
		CHECK_FALSE(host.FinishDialog(*token, request));
		const auto closing = host.BeginDialog(*token, 0);
		REQUIRE(closing > request);
		REQUIRE(host.Close(*token));
		CHECK_FALSE(host.SetSliderParameter(closing, 0, 42));
		CHECK_FALSE(host.FinishDialog(*token, closing));
		previous = closing;
	}
}

TEST_CASE("Native host builds controls transactionally with Classic option IDs")
{
	NativeHostSession host;
	host.Reset(1);
	const auto token = host.Open(1, "mod");
	REQUIRE(token);
	REQUIRE(host.BeginPage(*token, "General", 0));
	REQUIRE(host.SetCursor(*token, 0, 2));
	CHECK(host.AddOption(*token, 3, "Enabled", "", 1, 0, "enabled") == 256);
	CHECK(host.AddOption(*token, 4, "Size", "{0}", 10, 1, "size") == 258);
	CHECK_FALSE(host.Read());
	REQUIRE(host.Publish(*token));
	const auto first = host.Read();
	REQUIRE(first);
	CHECK(first->buffers.optionFlags[2] == 260);
	CHECK(first->buffers.stateNames[0] == "enabled");
	REQUIRE(host.BeginPage(*token, "Other", 1));
	CHECK(host.Read() == first);
	CHECK(host.AddOption(*token, 2, "Text", "Value", 0, 0, "") == 512);
	REQUIRE(host.Publish(*token));
	CHECK(host.Read()->page == "Other");
	CHECK(first->page == "General");
}

TEST_CASE("Native host updates values without changing structural revision")
{
	NativeHostSession host;
	host.Reset(1);
	const auto token = host.Open(1, "mod");
	REQUIRE(token);
	REQUIRE(host.BeginPage(*token, "", -1));
	REQUIRE(host.AddOption(*token, 3, "Enabled", "", 0, 0, "") == 0);
	REQUIRE(host.Publish(*token));
	const auto first = host.Read();
	REQUIRE(host.SetValue(*token, 0, "", 1));
	REQUIRE(host.SetFlags(*token, 0, 1));
	CHECK(host.Read() == first);
	REQUIRE(host.Publish(*token));
	CHECK(host.Read()->structureRevision == first->structureRevision);
	CHECK(host.Read()->valueRevision == first->valueRevision + 1);
	const auto changed = host.Read();
	REQUIRE(host.Publish(*token));
	CHECK(host.Read() == changed);
	REQUIRE(host.Close(*token));
	CHECK(host.Read() == changed);
	CHECK_FALSE(host.SetValue(*token, 0, "", 0));
}

TEST_CASE("Native host rejects stale calls and invalid buffer imports")
{
	NativeHostSession host;
	host.Reset(1);
	const auto old = host.Open(1, "mod");
	REQUIRE(old);
	CHECK_FALSE(host.Open(1, "other"));
	host.Reset(2);
	CHECK_FALSE(host.Open(1, "mod"));
	const auto token = host.Open(2, "mod");
	REQUIRE(token);
	CHECK(*token != *old);
	CHECK_FALSE(host.BeginPage(*old, "", -1));
	CHECK_FALSE(host.Close(*old));
	REQUIRE(host.BeginPage(*token, "", -1));
	CHECK_FALSE(host.ImportBuffers(*token, {}));
	CHECK(host.AddOption(*token, 3, "Enabled", "", 0, 0, "state") == 0);
	CHECK(host.AddOption(*token, 3, "Duplicate", "", 0, 0, "STATE") == -1);
	CHECK_FALSE(host.SetValue(*token, 0, "", 1));
	REQUIRE(host.Publish(*token));
	CHECK_FALSE(host.SetValue(*token, 256, "", 1));
	CHECK_FALSE(host.SetFlags(*token, 0, -1));
	auto imported = host.Read()->buffers;
	imported.numericValues[0] = 1;
	REQUIRE(host.ImportBuffers(*token, imported));
	REQUIRE(host.Publish(*token));
	CHECK(host.Read()->buffers.numericValues[0] == 1);
}

TEST_CASE("Native host preserves Classic page capacity without limiting mod registrations")
{
	NativeHostSession host;
	host.Reset(1);
	for (int mod = 0; mod < 2233; ++mod) {
		const auto token = host.Open(1, std::to_string(mod));
		REQUIRE(token);
		REQUIRE(host.BeginPage(*token, "", -1));
		for (int index = 0; index < 128; ++index)
			REQUIRE(host.AddOption(*token, 3, "Toggle", "", 0, 0, "") == index);
		CHECK(host.AddOption(*token, 3, "Overflow", "", 0, 0, "") == -1);
		REQUIRE(host.Publish(*token));
		REQUIRE(host.Close(*token));
	}
}
