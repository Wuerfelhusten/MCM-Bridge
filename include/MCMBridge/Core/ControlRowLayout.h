#pragma once

#include <algorithm>
#include <string>
#include <string_view>

namespace MCMBridge
{
	struct ControlRowLayout
	{
		float widgetOffset{};
		float widgetWidth{};
		bool  stacked{};
	};

	inline ControlRowLayout FitControlRow(float a_width, float a_labelWidth, float a_preferredWidth, float a_reservedWidth, float a_spacing, bool a_multiline = false)
	{
		const auto width = (std::max)(0.0F, a_width);
		const auto widgetWidth = (std::clamp)(a_preferredWidth, 0.0F, (std::max)(0.0F, width - a_reservedWidth));
		const auto offset = (std::max)(0.0F, width - a_reservedWidth - widgetWidth);
		const auto stacked = a_multiline || (a_labelWidth > 0 && a_labelWidth + a_spacing > offset);
		return { stacked ? 0.0F : offset, stacked ? (std::max)(0.0F, width - a_reservedWidth) : widgetWidth, stacked };
	}

	inline float FitMCMContentScale(float a_width, float a_fontSize)
	{
		if (a_fontSize <= 0)
			return 1;
		return (std::clamp)(a_width / (a_fontSize * 40), 0.8F, 1.1F);
	}

	// Display shortening never participates in control/page identity. Trim only
	// at UTF-8 boundaries so translated labels remain valid.
	template <class Measure>
	std::string FitControlText(std::string_view a_text, float a_width, Measure a_measure)
	{
		if (a_width <= 0)
			return {};
		if (a_measure(a_text) <= a_width)
			return std::string(a_text);
		constexpr std::string_view suffix = "...";
		if (a_measure(suffix) > a_width)
			return {};
		auto end = a_text.size();
		while (end > 0) {
			--end;
			while (end > 0 && (static_cast<unsigned char>(a_text[end]) & 0xC0U) == 0x80U)
				--end;
			const auto candidate = std::string(a_text.substr(0, end)) + std::string(suffix);
			if (a_measure(candidate) <= a_width)
				return candidate;
		}
		return std::string(suffix);
	}
}
