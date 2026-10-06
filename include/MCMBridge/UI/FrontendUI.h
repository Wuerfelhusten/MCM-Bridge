#pragma once

#include "MCMBridge/Framework/RenderContext.h"
#include "SKSEMenuFramework.h"

namespace MCMBridge::FrontendUI
{
	// These are value types only. Backend-owned fonts, draw lists and viewports
	// must never be passed to the other frontend's ImGui context.
	using namespace ImGuiMCP;
	using ImVec2 = ImGuiMCP::ImVec2;
	using ImVec4 = ImGuiMCP::ImVec4;
	struct Style
	{
		ImVec2 FramePadding;
		ImVec2 ItemSpacing;
	};
	const Style* GetStyle();
	ImVec2       DisplayCenter();
	ImVec2       GetContentRegionAvail();
	ImVec2       CalcTextSize(const char* a_text);
	float        GetCursorPosX();
	float        GetFontSize();
	float        GetFrameHeight();
	float        GetFrameHeightWithSpacing();
	float        GetTextLineHeightWithSpacing();
	void         SetCursorPosX(float a_x);
	void         SetNextItemWidth(float a_width);
	void         SetNextWindowSize(ImVec2 a_size, int a_condition = 0);
	void         SetNextWindowPos(ImVec2 a_position, int a_condition = 0, ImVec2 a_pivot = {});
	void         SetNextWindowFocus();
	bool         Begin(const char* a_title, bool* a_open = nullptr, int a_flags = 0);
	void         End();
	bool         BeginChild(const char* a_id, ImVec2 a_size = {}, int a_flags = 0);
	void         EndChild();
	void         BeginGroup();
	void         EndGroup();
	void         BeginDisabled(bool a_disabled = true);
	void         EndDisabled();
	void         SameLine(float a_offset = 0, float a_spacing = -1);
	void         Separator();
	void         Spacing();
	void         PushID(int a_id);
	void         PopID();
	void         PushStyleColor(int a_color, ImVec4 a_value);
	void         PopStyleColor(int a_count = 1);
	void         PushStyleVar(int a_var, ImVec2 a_value);
	void         PopStyleVar(int a_count = 1);
	void         PushTextWrapPos(float a_position = 0);
	void         PopTextWrapPos();
	bool         Button(const char* a_label, ImVec2 a_size = {});
	bool         Checkbox(const char* a_label, bool* a_value);
	bool         InputText(const char* a_label, char* a_buffer, std::size_t a_size, int a_flags = 0);
	bool         SliderFloat(const char* a_label, float* a_value, float a_min, float a_max, const char* a_format = "%.3f");
	bool         SliderInt(const char* a_label, int* a_value, int a_min, int a_max, const char* a_format = "%d");
	bool         DragFloat(const char* a_label, float* a_value, float a_speed = 1, float a_min = 0, float a_max = 0, const char* a_format = "%.3f");
	bool         ColorEdit4(const char* a_label, float* a_color, int a_flags = 0);
	bool         Selectable(const char* a_label, bool a_selected = false);
	bool         Combo(const char* a_label, int* a_selected, const char* const* a_items, int a_count);
	bool         BeginCombo(const char* a_id, const char* a_preview);
	void         EndCombo();
	bool         TreeNode(const char* a_label);
	void         TreePop();
	bool         CollapsingHeader(const char* a_label);
	bool         BeginTable(const char* a_id, int a_columns, int a_flags = 0);
	void         EndTable();
	void         TableSetupColumn(const char* a_label, int a_flags = 0, float a_width = 0);
	void         TableNextRow(int a_flags = 0, float a_height = 0);
	void         TableSetColumnIndex(int a_index);
	bool         IsItemHovered();
	bool         IsItemDeactivatedAfterEdit();
	void         SetItemDefaultFocus();
	void         TextUnformatted(const char* a_text, const char* a_end = nullptr);
	void         Text(const char* a_format, ...);
	void         TextWrapped(const char* a_format, ...);
	void         TextDisabled(const char* a_format, ...);
	void         SetItemTooltip(const char* a_format, ...);
}

namespace BridgeUI = MCMBridge::FrontendUI;
