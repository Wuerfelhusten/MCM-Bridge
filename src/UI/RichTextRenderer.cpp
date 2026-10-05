#include "MCMBridge/UI/RichTextRenderer.h"

#include "SKSEMenuFramework.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
	ImGuiMCP::ImVec4 TextColor(std::uint32_t a_color)
	{
		return {
			static_cast<float>((a_color >> 16U) & 0xFFU) / 255.0F,
			static_cast<float>((a_color >> 8U) & 0xFFU) / 255.0F,
			static_cast<float>(a_color & 0xFFU) / 255.0F,
			1.0F
		};
	}

	void DrawSpans(const MCMBridge::SkyUIRichText& a_text, ImGuiMCP::ImVec2 a_position, ImGuiMCP::ImGuiCol a_fallback)
	{
		auto*      drawList = ImGuiMCP::GetWindowDrawList();
		auto*      font = ImGuiMCP::GetFont();
		const auto fontSize = ImGuiMCP::GetFontSize();
		const auto lineStart = a_position.x;
		for (const auto& span : a_text.spans) {
			const auto       color = span.color ? ImGuiMCP::GetColorU32(TextColor(*span.color)) : ImGuiMCP::GetColorU32(a_fallback);
			std::string_view remaining = span.text;
			while (!remaining.empty()) {
				const auto newline = remaining.find('\n');
				const auto line = remaining.substr(0, newline);
				if (!line.empty()) {
					ImGuiMCP::ImDrawListManager::AddText(drawList, a_position, color, line.data(), line.data() + line.size());
					a_position.x += ImGuiMCP::ImFontManger::CalcTextSizeA(
						font, fontSize, std::numeric_limits<float>::max(), 0.0F,
						line.data(), line.data() + line.size(), nullptr)
					                    .x;
				}
				if (newline == std::string_view::npos)
					break;
				a_position.x = lineStart;
				a_position.y += fontSize;
				remaining.remove_prefix(newline + 1);
			}
		}
	}

	void RenderText(const MCMBridge::SkyUIRichText& a_text, ImGuiMCP::ImGuiCol a_fallback)
	{
		if (a_text.spans.size() <= 1) {
			const auto color = !a_text.spans.empty() && a_text.spans.front().color ?
			                       TextColor(*a_text.spans.front().color) :
			                       *ImGuiMCP::GetStyleColorVec4(a_fallback);
			ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, color);
			ImGuiMCP::TextUnformatted(a_text.plainText.c_str());
			ImGuiMCP::PopStyleColor();
			return;
		}

		// Reserve one text item so inline colors do not change layout or hover bounds.
		ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, ImGuiMCP::ImVec4{});
		ImGuiMCP::TextUnformatted(a_text.plainText.c_str());
		ImGuiMCP::PopStyleColor();
		if (ImGuiMCP::IsItemVisible())
			DrawSpans(a_text, ImGuiMCP::GetItemRectMin(), a_fallback);
	}
}

namespace MCMBridge::RichTextRenderer
{
	void Render(std::string_view a_source)
	{
		RenderText(ParseSkyUIRichText(a_source), ImGuiMCP::ImGuiCol_Text);
	}

	void RenderDisabled(const SkyUIRichText& a_text)
	{
		RenderText(a_text, ImGuiMCP::ImGuiCol_TextDisabled);
	}

	void RenderHeader(std::string_view a_source)
	{
		const auto text = ParseSkyUIRichText(a_source);
		const auto mixed = text.spans.size() > 1;
		const auto color = mixed                                           ? ImGuiMCP::ImVec4{} :
		                   !text.spans.empty() && text.spans.front().color ? TextColor(*text.spans.front().color) :
		                                                                     *ImGuiMCP::GetStyleColorVec4(ImGuiMCP::ImGuiCol_Text);
		ImGuiMCP::PushStyleColor(ImGuiMCP::ImGuiCol_Text, color);
		ImGuiMCP::SeparatorTextEx(0, text.plainText.c_str(), text.plainText.c_str() + text.plainText.size(), 0.0F);
		ImGuiMCP::PopStyleColor();
		if (!mixed || !ImGuiMCP::IsItemVisible())
			return;

		const auto             minimum = ImGuiMCP::GetItemRectMin();
		const auto             maximum = ImGuiMCP::GetItemRectMax();
		const auto             labelSize = ImGuiMCP::CalcTextSize(text.plainText.c_str());
		const auto*            style = ImGuiMCP::GetStyle();
		const auto             available = (std::max)(0.0F, maximum.x - minimum.x - 2.0F * style->SeparatorTextPadding.x);
		const ImGuiMCP::ImVec2 position{
			minimum.x + style->SeparatorTextPadding.x +
				(std::max)(0.0F, (available - labelSize.x) * style->SeparatorTextAlign.x),
			minimum.y + std::ceil((maximum.y - minimum.y - labelSize.y) * style->SeparatorTextAlign.y)
		};
		auto* drawList = ImGuiMCP::GetWindowDrawList();
		ImGuiMCP::ImDrawListManager::PushClipRect(drawList, minimum, maximum, true);
		DrawSpans(text, position, ImGuiMCP::ImGuiCol_Text);
		ImGuiMCP::ImDrawListManager::PopClipRect(drawList);
	}
}
