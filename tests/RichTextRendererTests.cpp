#include "MCMBridge/UI/RichTextRenderer.h"
#include "SKSEMenuFramework.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
	using namespace ImGuiMCP;
	using namespace MCMBridge::RichTextRenderer;
}

TEST_CASE("MCM header colors preserve the separator theme and do not leak")
{
	Test::Reset();
	const auto theme = GetStyle()->Colors;
	RenderHeader("<font color='#CDAF6E'>General</font>");
	RenderHeader("<font color='#11ABA1'>Attacking</font>");
	RenderHeader("Plain header");
	REQUIRE(Test::state.items.size() == 3);
	CHECK(Test::state.items[0].text == "General");
	CHECK(Test::state.items[0].color == 0xFF6EAFCDU);
	CHECK(Test::state.items[1].text == "Attacking");
	CHECK(Test::state.items[1].color == 0xFFA1AB11U);
	CHECK(Test::state.items[2].color == GetColorU32(ImGuiCol_Text));
	for (const auto& item : Test::state.items) {
		CHECK(item.separator);
		CHECK(item.separatorColor == GetColorU32(ImGuiCol_Separator));
	}
	CHECK(GetStyle()->Colors == theme);
	CHECK(Test::state.colors.empty());
}

TEST_CASE("MCM label colors restore nested font colors without changing the text item")
{
	Test::Reset();
	Render("<font color='#CDAF6E'>A<font color='#11ABA1'>B</font>C</font>D");
	REQUIRE(Test::state.items.size() == 1);
	CHECK(Test::state.items[0].text == "ABCD");
	REQUIRE(Test::state.draws.size() == 4);
	CHECK(Test::state.draws[0].color == 0xFF6EAFCDU);
	CHECK(Test::state.draws[1].color == 0xFFA1AB11U);
	CHECK(Test::state.draws[2].color == 0xFF6EAFCDU);
	CHECK(Test::state.draws[3].color == GetColorU32(ImGuiCol_Text));
	CHECK(Test::state.draws[0].position.x == 10.0F);
	CHECK(Test::state.draws[1].position.x == 18.0F);
	CHECK(Test::state.draws[2].position.x == 26.0F);
	CHECK(Test::state.draws[3].position.x == 34.0F);
	CHECK(Test::state.colors.empty());
}

TEST_CASE("Mixed-color headers retain header alignment and clipping")
{
	Test::Reset();
	RenderHeader("A<font color='#11ABA1'>B</font>C");
	REQUIRE(Test::state.items.size() == 1);
	CHECK(Test::state.items[0].text == "ABC");
	REQUIRE(Test::state.draws.size() == 3);
	CHECK(Test::state.draws[0].position.x == 20.0F);
	CHECK(Test::state.draws[0].position.y == 23.0F);
	CHECK(Test::state.draws[1].color == 0xFFA1AB11U);
	for (const auto& draw : Test::state.draws) CHECK(draw.clipped);
	CHECK(Test::state.clipDepth == 0);
	CHECK(Test::state.intersected);
	CHECK(Test::state.colors.empty());
}

TEST_CASE("MCM colors honor disabled alpha and uncolored values retain their theme")
{
	Test::Reset();
	GetStyle()->Alpha = 0.4F;
	Render("<font color='#11ABA1'>Disabled label</font>");
	RenderDisabled(MCMBridge::ParseSkyUIRichText("Uncolored value"));
	RenderDisabled(MCMBridge::ParseSkyUIRichText("Status: <font color='#00FF00'>Ready</font>"));
	REQUIRE(Test::state.items.size() == 3);
	CHECK(Test::state.items[0].color == 0x66A1AB11U);
	CHECK(Test::state.items[1].color == GetColorU32(ImGuiCol_TextDisabled));
	REQUIRE(Test::state.draws.size() == 2);
	CHECK(Test::state.draws[0].color == GetColorU32(ImGuiCol_TextDisabled));
	CHECK(Test::state.draws[1].color == 0x6600FF00U);
	CHECK(Test::state.colors.empty());
}

TEST_CASE("Colored text retains line breaks and ordinary percent characters")
{
	Test::Reset();
	Render("100%<br><font color='#00FF00'>Next</font> line");
	REQUIRE(Test::state.items.size() == 1);
	CHECK(Test::state.items[0].text == "100%\nNext line");
	REQUIRE(Test::state.draws.size() == 3);
	CHECK(Test::state.draws[1].text == "Next");
	CHECK(Test::state.draws[1].position.x == 10.0F);
	CHECK(Test::state.draws[1].position.y == 40.0F);
	CHECK(Test::state.draws[2].position.x == 42.0F);
	CHECK(Test::state.draws[2].position.y == 40.0F);
}

TEST_CASE("Invalid header colors fall back to the theme without exposing markup")
{
	Test::Reset();
	RenderHeader("<font color='#bad'>Fallback &amp; safe</font>");
	REQUIRE(Test::state.items.size() == 1);
	CHECK(Test::state.items[0].text == "Fallback & safe");
	CHECK(Test::state.items[0].color == GetColorU32(ImGuiCol_Text));
	CHECK(Test::state.colors.empty());
}

TEST_CASE("Clipped mixed-color headers restore style without submitting text draws")
{
	Test::Reset();
	Test::state.visible = false;
	const auto theme = GetStyle()->Colors;
	RenderHeader("A<font color='#11ABA1'>B</font>");
	CHECK(Test::state.draws.empty());
	CHECK(Test::state.clipDepth == 0);
	CHECK(Test::state.colors.empty());
	CHECK(GetStyle()->Colors == theme);
}
