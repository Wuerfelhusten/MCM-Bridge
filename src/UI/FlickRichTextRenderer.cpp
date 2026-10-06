#include "MCMBridge/UI/FlickRichTextRenderer.h"

#include "MCMBridge/Core/RichTextLayout.h"

#include "FUCK_API.h"

#include <cmath>

namespace
{
	using Lines = std::vector<std::vector<MCMBridge::SkyUIRichTextSpan>>;

	float Measure(std::string_view a_text)
	{
		float width, height;
		FUCK::GetInterface()->CalcTextSize(a_text.data(), a_text.data() + a_text.size(), false, -1, &width, &height);
		return width;
	}

	void DrawLines(const Lines& a_lines, bool a_disabled, ImVec2 a_origin, float a_spacingX)
	{
		auto& api = *FUCK::GetInterface();
		api.PushStyleVarVec(ImGuiStyleVar_ItemSpacing, { a_spacingX, 0 });
		for (std::size_t index = 0; index < a_lines.size(); ++index) {
			api.SetCursorPos(a_origin.x, a_origin.y + static_cast<float>(index) * api.GetTextLineHeight());
			const auto& line = a_lines[index];
			bool        sameLine{};
			if (line.empty())
				api.TextUnformatted("", nullptr);
			for (const auto& span : line) {
				if (sameLine)
					api.SameLine(0, 0);
				if (span.color) {
					const auto color = *span.color;
					api.PushStyleColor(ImGuiCol_Text, { static_cast<float>((color >> 16) & 255) / 255, static_cast<float>((color >> 8) & 255) / 255, static_cast<float>(color & 255) / 255, 1 });
				} else if (a_disabled) {
					ImVec4 color;
					api.GetStyleColorVec4(ImGuiCol_TextDisabled, &color.x, &color.y, &color.z, &color.w);
					api.PushStyleColor(ImGuiCol_Text, color);
				}
				api.TextUnformatted(span.text.c_str(), nullptr);
				if (span.color || a_disabled)
					api.PopStyleColor(1);
				sameLine = true;
			}
		}
		api.PopStyleVar(1);
	}
}

namespace MCMBridge::FlickRichTextRenderer
{
	void Draw(const SkyUIRichText& a_text, bool a_disabled, bool a_header)
	{
		auto& api = *FUCK::GetInterface();
		float width, availableHeight, spacingX, spacingY;
		api.GetContentRegionAvail(&width, &availableHeight);
		api.GetStyleVarVec(ImGuiStyleVar_ItemSpacing, &spacingX, &spacingY);
		ImVec2 origin, screen, padding;
		api.GetCursorPos(&origin.x, &origin.y);
		api.GetCursorScreenPos(&screen.x, &screen.y);
		if (a_header) {
			api.GetStyleVarVec(ImGuiStyleVar_SeparatorTextPadding, &padding.x, &padding.y);
			padding.x = std::clamp(padding.x, 0.0F, (std::max)(0.0F, width * 0.25F));
			padding.y = (std::max)(0.0F, padding.y);
		}
		const auto lines = WrapRichText(a_text, (std::max)(1.0F, width - 2 * padding.x), Measure);
		api.BeginGroup();
		if (!a_header) {
			DrawLines(lines, a_disabled, origin, spacingX);
			api.EndGroup();
			return;
		}

		// Match SMF's SeparatorText layout, retaining literal ## and per-span
		// colors. FLICK's SeparatorText entry point hides ## and cannot draw
		// mixed-color labels. Its scalar style getter also omits border size.
		const auto thickness = 3.0F * api.GetResolutionScale();
		const auto textHeight = a_text.plainText.empty() ? 0.0F : static_cast<float>(lines.size()) * api.GetTextLineHeight();
		const auto height = (std::max)(textHeight + 2 * padding.y, thickness);
		api.Dummy((std::max)(0.0F, width), height);
		ImVec2 next;
		api.GetCursorPos(&next.x, &next.y);
		if (!a_text.plainText.empty())
			DrawLines(lines, a_disabled, { origin.x + padding.x, origin.y + padding.y }, spacingX);

		float labelWidth{};
		for (const auto& line : lines) {
			std::string label;
			for (const auto& span : line)
				label += span.text;
			labelWidth = (std::max)(labelWidth, Measure(label));
		}
		ImVec4 color;
		api.GetStyleColorVec4(ImGuiCol_Separator, &color.x, &color.y, &color.z, &color.w);
		color.w *= api.GetStyleVar(ImGuiStyleVar_Alpha);
		const auto middle = std::ceil(screen.y + height * 0.5F);
		const auto line = [&](float a_start, float a_end) {
			if (a_end > a_start)
				api.DrawLine({ a_start, middle }, { a_end, middle }, color, thickness);
		};
		if (a_text.plainText.empty()) {
			line(screen.x, screen.x + width);
		} else {
			line(screen.x, screen.x + padding.x - spacingX);
			line(screen.x + padding.x + labelWidth + spacingX, screen.x + width);
		}
		// Text is overlaid in an already reserved header row. Restore its ending
		// cursor without adding a second ItemSpacing gap or shifting the column.
		api.SetCursorPos(origin.x, next.y - spacingY);
		api.Dummy(0, 0);
		api.EndGroup();
	}
}
