#include "MCMBridge/UI/FlickWidgetDecoration.h"
#include "MCMBridge/UI/FrontendUI.h"
#include "MCMBridge/UI/IconButton.h"

// Declare the frontend-local value types before global ImGui declarations.
#include "FUCK_API.h"
#include "imgui_internal.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

namespace
{
	struct DrawState
	{
		std::string              label;
		std::string              popup;
		std::string              tooltip;
		::ImVec2                 size;
		::ImVec2                 cursor;
		::ImVec2                 minimum;
		::ImVec2                 maximum;
		::ImVec2                 borderMaximum;
		::ImVec2                 imageMinimum;
		::ImVec2                 imageMaximum;
		::ImVec2                 groupMinimum;
		::ImVec2                 groupMaximum;
		float                    textBaseOffset{};
		int                      buttonFlags{};
		int                      images{};
		int                      imageLoads{};
		int                      imageReleases{};
		int                      selectableFlags{};
		float                    width{ 400 };
		float                    itemWidth{ 128 };
		int                      borders{};
		int                      closes{};
		int                      lines{};
		int                      circles{};
		int                      triangles{};
		int                      sliders{};
		int                      releaseChecks{};
		int                      combos{};
		std::vector<std::string> options;
		bool                     hovered{};
		bool                     popupOpen{};
	};
	DrawState state;

	class FlickScope
	{
	public:
		FlickScope() : previous(FUCK::GetInterface()), context(MCMBridge::Frontend::kFlick)
		{
			state = {};
			api.GetContentRegionAvail = +[](float* a_x, float* a_y) { *a_x = state.width; *a_y = 600; };
			api.GetStyleVarVec = +[](ImGuiStyleVar, float* a_x, float* a_y) { *a_x = 8; *a_y = 4; };
			api.CalcTextSize = +[](const char* a_text, const char* a_end, bool, float, float* a_x, float* a_y) {
				*a_x = static_cast<float>(a_end ? a_end - a_text : std::char_traits<char>::length(a_text)) * 8;
				*a_y = 24;
			};
			api.GetFrameHeight = +[] { return 32.0F; };
			api.Selectable = +[](const char* a_label, bool, int a_flags, const ::ImVec2& a_size) {
				state.label = a_label;
				state.size = a_size;
				state.selectableFlags = a_flags;
				const auto pad = (a_flags & ImGuiSelectableFlags_NoPadWithHalfSpacing) ? 0.0F : 4.0F;
				state.minimum = { state.cursor.x - pad, state.cursor.y + state.textBaseOffset - pad };
				state.maximum = { state.cursor.x + a_size.x + pad, state.cursor.y + state.textBaseOffset + a_size.y + pad };
				return true;
			};
			api.InvisibleButton = +[](const char* a_label, const ::ImVec2& a_size, int a_flags) {
				state.label = a_label;
				state.size = a_size;
				state.buttonFlags = a_flags;
				state.minimum = state.cursor;
				state.maximum = { state.cursor.x + a_size.x, state.cursor.y + a_size.y };
				return true;
			};
			api.LoadImage = +[](const char* a_path, bool a_resize) -> void* {
				CHECK(std::string_view(a_path) == "Data/Interface/MCMBridge/reset.png");
				CHECK_FALSE(a_resize);
				++state.imageLoads;
				return &state;
			};
			api.GetImageInfo = +[](void*, float* a_width, float* a_height) { *a_width = 256; *a_height = 248; };
			api.ReleaseImage = +[](void* a_image) { CHECK(a_image == &state); ++state.imageReleases; };
			api.AddImage = +[](void* a_image, const ::ImVec2& a_minimum, const ::ImVec2& a_maximum, const ::ImVec2&, const ::ImVec2&, const ::ImVec4&) {
				CHECK(a_image == &state);
				++state.images;
				state.imageMinimum = a_minimum;
				state.imageMaximum = a_maximum;
			};
			api.GetItemRectMin = +[](float* a_x, float* a_y) { *a_x = state.minimum.x; *a_y = state.minimum.y; };
			api.GetItemRectMax = +[](float* a_x, float* a_y) { *a_x = state.maximum.x; *a_y = state.maximum.y; };
			api.GetCursorPos = +[](float* a_x, float* a_y) { *a_x = state.cursor.x; *a_y = state.cursor.y; };
			api.GetCursorScreenPos = +[](float* a_x, float* a_y) { *a_x = state.cursor.x + 100; *a_y = state.cursor.y + 200; };
			api.SetCursorPos = +[](float a_x, float a_y) { state.cursor = { a_x, a_y }; };
			api.SameLine = +[](float, float) {};
			api.IsItemHovered = +[](int) { return state.hovered; };
			api.GetStyleColorVec4 = +[](ImGuiCol, float* a_x, float* a_y, float* a_z, float* a_w) { *a_x = 1; *a_y = 1; *a_z = 1; *a_w = 1; };
			api.GetStyleVar = +[](ImGuiStyleVar) { return 1.0F; };
			api.DrawRect = +[](const ::ImVec2&, const ::ImVec2& a_maximum, const ::ImVec4&, float, float) { ++state.borders; state.borderMaximum = a_maximum; };
			api.DrawLine = +[](const ::ImVec2&, const ::ImVec2&, const ::ImVec4&, float) { ++state.lines; };
			api.DrawCircleFilled = +[](const ::ImVec2&, float, const ::ImVec4&, int) { ++state.circles; };
			api.DrawTriangleFilled = +[](const ::ImVec2&, const ::ImVec2&, const ::ImVec2&, const ::ImVec4&) { ++state.triangles; };
			api.SliderFloat = +[](const char*, float*, float, float, const char*) { ++state.sliders; state.size = { state.itemWidth, 32 }; return true; };
			api.SliderInt = +[](const char*, int*, int, int, const char*) { ++state.sliders; state.size = { state.itemWidth, 32 }; return true; };
			api.IsItemDeactivatedAfterEdit = +[] { ++state.releaseChecks; return true; };
			api.Combo = +[](const char* a_label, int* a_selected, const char* const* a_items, int a_count) {
				++state.combos;
				state.label = a_label;
				state.options.clear();
				for (int index = 0; index < a_count; ++index)
					state.options.emplace_back(a_items[index]);
				*a_selected = a_count - 1;
				state.groupMinimum = { state.cursor.x + 100, state.cursor.y + 200 };
				state.groupMaximum = { state.groupMinimum.x + state.itemWidth, state.groupMinimum.y + 40 };
				state.minimum = state.groupMinimum;
				state.maximum = state.groupMaximum;
				if (state.popupOpen) {
					state.minimum = { 1000, 1000 };
					state.maximum = { 1100, 1024 };
				}
				return true;
			};
			api.BeginGroup = +[] {};
			api.EndGroup = +[] { state.minimum = state.groupMinimum; state.maximum = state.groupMaximum; };
			api.SetTooltip = +[](const char* a_text) { state.tooltip = a_text; };
			api.CalcItemWidth = +[] { return state.itemWidth; };
			api.SetNextItemWidth = +[](float a_width) { state.itemWidth = a_width; };
			api.OpenPopup = +[](const char* a_id, int) { state.popup = a_id; };
			api.BeginPopup = +[](const char*, int) { return true; };
			api.EndPopup = +[] {};
			api.CloseCurrentPopup = +[] { ++state.closes; };
			api.PushStyleVarVec = +[](ImGuiStyleVar, const ::ImVec2&) {};
			api.PopStyleVar = +[](int) {};
			FUCK::GetInterface() = &api;
		}
		~FlickScope()
		{
			MCMBridge::FlickWidgetDecoration::ReleaseIcon();
			FUCK::GetInterface() = previous;
		}
		FlickScope(const FlickScope&) = delete;
		FlickScope& operator=(const FlickScope&) = delete;

	private:
		FUCK_Interface*          previous;
		MCMBridge::RenderContext context;
		FUCK_Interface           api{};
	};
}

TEST_CASE("FLICK buttons respect supplied bounds rather than measuring hidden identities")
{
	FlickScope scope;
	CHECK(BridgeUI::Button("7 (NUMPAD)##very-long-control-identity", { 100, 32 }));
	CHECK(state.size.x == 100);
	CHECK(state.size.y == 32);
	CHECK(state.label.ends_with("###very-long-control-identity"));
	CHECK(state.borders == 1);
	CHECK((state.selectableFlags & ImGuiSelectableFlags_NoPadWithHalfSpacing) != 0);
	CHECK(state.maximum.x == 100);
	CHECK(state.borderMaximum.x == 99.5F);
	state.width = 60;
	state.hovered = true;
	CHECK(BridgeUI::Button("Very long translated hotkey##original-id", { 100, 32 }));
	CHECK(state.size.x == 60);
	CHECK(state.label.starts_with("Ve...###"));
	CHECK(state.tooltip == "Very long translated hotkey");
}

TEST_CASE("FLICK dropdowns keep their control width and close after selection")
{
	FlickScope scope;
	BridgeUI::SetNextItemWidth(90);
	CHECK(BridgeUI::BeginCombo("original-control-id", "A very long option"));
	CHECK(state.size.x == 90);
	CHECK(state.popup == "original-control-id");
	CHECK(state.triangles == 1);
	CHECK(BridgeUI::Selectable("Another option##raw-option-id"));
	CHECK(state.closes == 1);
	BridgeUI::EndCombo();
	CHECK(BridgeUI::Selectable("Not a popup"));
	CHECK(state.closes == 1);
}

TEST_CASE("FLICK reset and unmap actions do not require the SMF icon font")
{
	FlickScope scope;
	CHECK(MCMBridge::IconButton::Render("private-icon", "reset-control-id"));
	CHECK(state.label == "##reset-control-id");
	CHECK(state.size.x == 32);
	CHECK(state.lines == 0);
	CHECK(state.triangles == 0);
	CHECK(state.images == 1);
	CHECK(state.imageLoads == 1);
	CHECK(state.imageMaximum.x - state.imageMinimum.x == 32 * 0.65F);
	CHECK_THAT((state.imageMaximum.y - state.imageMinimum.y) / (state.imageMaximum.x - state.imageMinimum.x), Catch::Matchers::WithinAbs(248.0F / 256, 0.0001));
	CHECK(MCMBridge::IconButton::Render("private-icon", "reset-another"));
	CHECK(state.imageLoads == 1);
	MCMBridge::FlickWidgetDecoration::ReleaseIcon();
	CHECK(state.imageReleases == 1);
	CHECK(MCMBridge::IconButton::Render("private-icon", "clear-control-id"));
	CHECK(state.label == "##clear-control-id");
	CHECK(state.size.x == 32);
	CHECK(state.lines == 2);
}

TEST_CASE("FLICK action buttons center on actual widget heights and stay inside the column")
{
	FlickScope scope;
	for (const auto height : { 24.0F, 32.0F, 40.0F, 48.0F, 64.0F }) {
		CAPTURE(height);
		// Model a wrapped label followed by native widgets with their own padding.
		state.cursor = { 80, 150 };
		state.minimum = { 100, 300 };
		state.maximum = { 300, 300 + height };
		state.textBaseOffset = 7;
		MCMBridge::IconButton::AlignToLastWidget(400);
		CHECK(state.cursor.x == 368);
		CHECK(state.cursor.y + 16 == 100 + height * 0.5F);
		CHECK(MCMBridge::IconButton::Render("private-icon", "reset-aligned"));
		CHECK(state.maximum.x == 400);
		CHECK(state.borderMaximum.x == 399.5F);
		CHECK(state.minimum.y + 16 == 100 + height * 0.5F);
		CHECK((state.buttonFlags & ImGuiButtonFlags_EnableNav) != 0);
	}
}

TEST_CASE("FLICK dropdown actions center on the parent frame while the popup is open")
{
	FlickScope scope;
	for (const auto open : { false, true }) {
		state.popupOpen = open;
		state.textBaseOffset = 7;
		state.cursor = { 80, 150 };
		int         selected{};
		const char* items[]{ "First", "Second" };
		CHECK(BridgeUI::Combo("##dropdown", &selected, items, 2));
		MCMBridge::IconButton::AlignToLastWidget(400);
		CHECK(MCMBridge::IconButton::Render("private-icon", "reset-dropdown"));
		CHECK(state.minimum.y == 154);
		CHECK(state.maximum.y == 186);
		CHECK(state.maximum.x == 400);
		CHECK((state.imageMinimum.y + state.imageMaximum.y) * 0.5F == 170);
	}
}

TEST_CASE("FLICK sliders use exactly one native widget without additional drawing")
{
	FlickScope scope;
	float      value = 50;
	CHECK(BridgeUI::SliderFloat("##slider", &value, 0, 100));
	CHECK(value == 50);
	CHECK(state.sliders == 1);
	CHECK(BridgeUI::IsItemDeactivatedAfterEdit());
	CHECK(state.releaseChecks == 1);
	int integer = 100;
	CHECK(BridgeUI::SliderInt("##integer", &integer, 0, 100));
	CHECK(state.sliders == 2);
	CHECK(integer == 100);
	CHECK(state.lines == 0);
	CHECK(state.circles == 0);
	CHECK(state.triangles == 0);
	CHECK(state.borders == 0);
}

TEST_CASE("FLICK native dropdowns preserve index mapping for labels with structural-looking text")
{
	FlickScope  scope;
	int         selected{};
	const char* items[]{ "Normal", "##HEADER:literal", "Hash##value" };
	CHECK(BridgeUI::Combo("##original-control", &selected, items, 3));
	CHECK(state.combos == 1);
	CHECK(state.label == "##original-control");
	CHECK(selected == 2);
	CHECK(state.options == std::vector<std::string>{ "Normal", "# #HEADER:literal", "Hash# #value" });
	CHECK(std::string_view(items[1]) == "##HEADER:literal");
}
