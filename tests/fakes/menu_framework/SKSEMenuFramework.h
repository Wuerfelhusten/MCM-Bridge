#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ImGuiMCP
{
	struct ImVec2
	{
		float x{}, y{};
	};
	struct ImVec4
	{
		float x{}, y{}, z{}, w{};
		bool  operator==(const ImVec4&) const = default;
	};
	using ImU32 = std::uint32_t;
	using ImGuiID = std::uint32_t;
	enum ImGuiCol
	{
		ImGuiCol_Text,
		ImGuiCol_TextDisabled,
		ImGuiCol_Separator
	};
	struct ImDrawList
	{};
	struct ImFont
	{};
	struct ImGuiStyle
	{
		std::array<ImVec4, 3> Colors{ ImVec4{ 1, 1, 1, 1 }, ImVec4{ 0.3F, 0.3F, 0.3F, 1 }, ImVec4{ 0.5F, 0.5F, 0.5F, 1 } };
		float                 Alpha{ 1.0F };
		ImVec2                SeparatorTextPadding{ 10.0F, 3.0F };
		ImVec2                SeparatorTextAlign{ 0.0F, 0.5F };
	};
	namespace Test
	{
		struct Item
		{
			std::string text;
			ImU32       color{};
			bool        separator{};
			ImU32       separatorColor{};
		};
		struct Draw
		{
			std::string text;
			ImU32       color{};
			ImVec2      position;
			bool        clipped{};
		};
		struct State
		{
			ImGuiStyle                               style;
			std::vector<std::pair<ImGuiCol, ImVec4>> colors;
			std::vector<Item>                        items;
			std::vector<Draw>                        draws;
			ImDrawList                               drawList;
			ImFont                                   font;
			ImVec2                                   minimum{ 10.0F, 20.0F };
			ImVec2                                   maximum;
			bool                                     visible{ true };
			int                                      clipDepth{};
			bool                                     intersected{};
		};
		inline State  state;
		inline void   Reset() { state = {}; }
		inline ImVec2 Measure(std::string_view text)
		{
			float width{}, lineWidth{};
			float height = 20.0F;
			for (const auto character : text) {
				if (character == '\n') {
					width = (std::max)(width, lineWidth);
					lineWidth = 0.0F;
					height += 20.0F;
				} else if ((static_cast<unsigned char>(character) & 0xC0U) != 0x80U) {
					lineWidth += 8.0F;
				}
			}
			return { (std::max)(width, lineWidth), height };
		}
	}
	inline ImGuiStyle*   GetStyle() { return &Test::state.style; }
	inline const ImVec4* GetStyleColorVec4(ImGuiCol color) { return &GetStyle()->Colors[color]; }
	inline void          PushStyleColor(ImGuiCol color, const ImVec4 value)
	{
		Test::state.colors.emplace_back(color, GetStyle()->Colors[color]);
		GetStyle()->Colors[color] = value;
	}
	inline void PopStyleColor()
	{
		const auto [color, value] = Test::state.colors.back();
		GetStyle()->Colors[color] = value;
		Test::state.colors.pop_back();
	}
	inline ImU32 GetColorU32(const ImVec4 color)
	{
		const auto byte = [](float value) { return static_cast<ImU32>(std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F)); };
		return byte(color.x) | (byte(color.y) << 8U) | (byte(color.z) << 16U) | (byte(color.w * GetStyle()->Alpha) << 24U);
	}
	inline ImU32  GetColorU32(ImGuiCol color) { return GetColorU32(*GetStyleColorVec4(color)); }
	inline ImVec2 CalcTextSize(const char* text) { return Test::Measure(text); }
	inline void   TextUnformatted(const char* text)
	{
		const auto size = CalcTextSize(text);
		Test::state.maximum = { Test::state.minimum.x + size.x, Test::state.minimum.y + size.y };
		Test::state.items.push_back({ text, GetColorU32(ImGuiCol_Text) });
	}
	inline void SeparatorTextEx(ImGuiID, const char* label, const char* end, float)
	{
		const std::string text(label, end);
		Test::state.maximum = { 400.0F, Test::state.minimum.y + Test::Measure(text).y + 2.0F * GetStyle()->SeparatorTextPadding.y };
		Test::state.items.push_back({ text, GetColorU32(ImGuiCol_Text), true, GetColorU32(ImGuiCol_Separator) });
	}
	inline bool        IsItemVisible() { return Test::state.visible; }
	inline ImVec2      GetItemRectMin() { return Test::state.minimum; }
	inline ImVec2      GetItemRectMax() { return Test::state.maximum; }
	inline ImDrawList* GetWindowDrawList() { return &Test::state.drawList; }
	inline ImFont*     GetFont() { return &Test::state.font; }
	inline float       GetFontSize() { return 20.0F; }
	namespace ImDrawListManager
	{
		inline void AddText(ImDrawList*, const ImVec2 position, ImU32 color, const char* begin, const char* end)
		{
			Test::state.draws.push_back({ std::string(begin, end), color, position, Test::state.clipDepth > 0 });
		}
		inline void PushClipRect(ImDrawList*, ImVec2, ImVec2, bool intersect)
		{
			++Test::state.clipDepth;
			Test::state.intersected = intersect;
		}
		inline void PopClipRect(ImDrawList*) { --Test::state.clipDepth; }
	}
	namespace ImFontManger
	{
		inline ImVec2 CalcTextSizeA(ImFont*, float, float, float, const char* begin, const char* end, const char**)
		{
			return Test::Measure(std::string_view(begin, end));
		}
	}
}
