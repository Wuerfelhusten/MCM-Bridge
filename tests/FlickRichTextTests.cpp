#include "MCMBridge/UI/FlickRichTextRenderer.h"

#include "FUCK_API.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

namespace
{
	struct Text
	{
		std::string text;
		ImVec2      position;
		ImVec4      color;
	};
	struct Line
	{
		ImVec2 first;
		ImVec2 last;
		ImVec4 color;
		float  thickness;
	};
	struct State
	{
		ImVec2              cursor;
		ImVec2              lastTextEnd;
		ImVec2              spacing{ 8, 4 };
		ImVec4              color{ 1, 1, 1, 1 };
		float               width{ 400 };
		float               scale{ 1 };
		int                 groups{};
		std::vector<Text>   texts;
		std::vector<Line>   lines;
		std::vector<ImVec4> colors;
		std::vector<ImVec2> styles;
	};
	State state;

	class Scope
	{
	public:
		Scope() : previous(FUCK::GetInterface())
		{
			state = {};
			api.GetContentRegionAvail = +[](float* a_x, float* a_y) { *a_x = state.width; *a_y = 600; };
			api.GetCursorPos = +[](float* a_x, float* a_y) { *a_x = state.cursor.x; *a_y = state.cursor.y; };
			api.GetCursorScreenPos = +[](float* a_x, float* a_y) { *a_x = state.cursor.x + 100; *a_y = state.cursor.y + 200; };
			api.SetCursorPos = +[](float a_x, float a_y) { state.cursor = { a_x, a_y }; };
			api.GetStyleVarVec = +[](ImGuiStyleVar a_var, float* a_x, float* a_y) {
				const auto value = a_var == ImGuiStyleVar_SeparatorTextPadding ? ImVec2{ 20, 3 } : state.spacing;
				*a_x = value.x;
				*a_y = value.y;
			};
			api.GetStyleVar = +[](ImGuiStyleVar) { return 0.75F; };
			api.GetStyleColorVec4 = +[](ImGuiCol a_color, float* a_x, float* a_y, float* a_z, float* a_w) {
				const auto color = a_color == ImGuiCol_Separator ? ImVec4{ 0.4F, 0.4F, 0.4F, 0.65F } : state.color;
				*a_x = color.x;
				*a_y = color.y;
				*a_z = color.z;
				*a_w = color.w;
			};
			api.GetResolutionScale = +[] { return state.scale; };
			api.GetTextLineHeight = +[] { return 24.0F; };
			api.CalcTextSize = +[](const char* a_text, const char* a_end, bool, float, float* a_x, float* a_y) {
				*a_x = static_cast<float>(a_end - a_text) * 8;
				*a_y = 24;
			};
			api.BeginGroup = +[] { ++state.groups; };
			api.EndGroup = +[] { --state.groups; };
			api.Dummy = +[](float, float a_height) { state.cursor = { 0, state.cursor.y + a_height + state.spacing.y }; };
			api.PushStyleVarVec = +[](ImGuiStyleVar, const ImVec2& a_value) { state.styles.push_back(state.spacing); state.spacing = a_value; };
			api.PopStyleVar = +[](int) { state.spacing = state.styles.back(); state.styles.pop_back(); };
			api.PushStyleColor = +[](ImGuiCol, const ImVec4& a_value) { state.colors.push_back(state.color); state.color = a_value; };
			api.PopStyleColor = +[](int) { state.color = state.colors.back(); state.colors.pop_back(); };
			api.TextUnformatted = +[](const char* a_text, const char*) {
				state.texts.push_back({ a_text, state.cursor, state.color });
				state.lastTextEnd = { state.cursor.x + static_cast<float>(std::char_traits<char>::length(a_text)) * 8, state.cursor.y };
				state.cursor = { 0, state.cursor.y + 24 + state.spacing.y };
			};
			api.SameLine = +[](float, float) { state.cursor = state.lastTextEnd; };
			api.DrawLine = +[](const ImVec2& a_first, const ImVec2& a_last, const ImVec4& a_color, float a_thickness) {
				state.lines.push_back({ a_first, a_last, a_color, a_thickness });
			};
			FUCK::GetInterface() = &api;
		}
		~Scope() { FUCK::GetInterface() = previous; }
		Scope(const Scope&) = delete;
		Scope& operator=(const Scope&) = delete;

	private:
		FUCK_Interface* previous;
		FUCK_Interface  api{};
	};
}

TEST_CASE("FLICK section separators flank the title at its midpoint with SMF spacing")
{
	Scope scope;
	MCMBridge::FlickRichTextRenderer::Draw(MCMBridge::ParseSkyUIRichText("Window Settings"), false, true);
	REQUIRE(state.texts.size() == 1);
	CHECK(state.texts.front().position.x == 20);
	CHECK(state.texts.front().position.y == 3);
	REQUIRE(state.lines.size() == 2);
	CHECK(state.lines[0].first.x == 100);
	CHECK(state.lines[0].last.x == 112);
	CHECK(state.lines[1].first.x == 248);
	CHECK(state.lines[1].last.x == 500);
	for (const auto& line : state.lines) {
		CHECK(line.first.y == 215);
		CHECK(line.last.y == 215);
		CHECK(line.thickness == 3);
		CHECK(line.color.x == 0.4F);
		CHECK_THAT(line.color.w, Catch::Matchers::WithinAbs(0.65F * 0.75F, 0.0001));
	}
	CHECK(state.cursor.x == 0);
	CHECK(state.cursor.y == 34);
	CHECK(state.groups == 0);
	CHECK(state.styles.empty());
	CHECK(state.colors.empty());
}

TEST_CASE("FLICK section separators retain inline colors and wrap inside the column")
{
	Scope scope;
	state.width = 130;
	MCMBridge::FlickRichTextRenderer::Draw(MCMBridge::ParseSkyUIRichText("Alpha <font color='#00FF00'>Beta Gamma</font>"), false, true);
	REQUIRE(state.texts.size() == 3);
	CHECK(state.texts[0].text == "Alpha ");
	CHECK(state.texts[1].text == "Beta");
	CHECK(state.texts[2].text == "Gamma");
	CHECK(state.texts[1].color.x == 0);
	CHECK(state.texts[1].color.y == 1);
	CHECK(state.texts[1].position.x == 68);
	CHECK(state.texts[2].position.x == 20);
	CHECK(state.texts[2].position.y == 27);
	REQUIRE(state.lines.size() == 2);
	CHECK(state.lines[1].first.x == 208);
	CHECK(state.lines[1].last.x == 230);
	CHECK(state.lines[1].first.y == 227);
	CHECK(state.cursor.y == 58);
	CHECK(state.colors.empty());
	CHECK(state.styles.empty());
}

TEST_CASE("FLICK header separators scale and preserve empty and literal titles")
{
	Scope scope;
	state.scale = 2;
	MCMBridge::FlickRichTextRenderer::Draw(MCMBridge::ParseSkyUIRichText(""), false, true);
	CHECK(state.texts.empty());
	REQUIRE(state.lines.size() == 1);
	CHECK(state.lines[0].first.x == 100);
	CHECK(state.lines[0].last.x == 500);
	CHECK(state.lines[0].thickness == 6);
	CHECK(state.cursor.y == 10);
	MCMBridge::FlickRichTextRenderer::Draw(MCMBridge::ParseSkyUIRichText("Header##literal"), false, true);
	REQUIRE(state.texts.size() == 1);
	CHECK(state.texts[0].text == "Header##literal");
}

TEST_CASE("FLICK ordinary rich text remains unadorned and narrow headers never span other columns")
{
	Scope scope;
	MCMBridge::FlickRichTextRenderer::Draw(MCMBridge::ParseSkyUIRichText("Ordinary text"), false, false);
	CHECK(state.lines.empty());
	CHECK(state.texts.front().position.x == 0);
	state.width = 50;
	MCMBridge::FlickRichTextRenderer::Draw(MCMBridge::ParseSkyUIRichText("Long heading"), false, true);
	for (const auto& line : state.lines) {
		CHECK(line.first.x >= 100);
		CHECK(line.last.x <= 150);
		CHECK(line.first.x < line.last.x);
	}
	CHECK(state.groups == 0);
	CHECK(state.styles.empty());
}
