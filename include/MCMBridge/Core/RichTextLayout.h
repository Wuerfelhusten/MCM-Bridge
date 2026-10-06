#pragma once

#include "MCMBridge/Core/SkyUIRichText.h"

#include <algorithm>

namespace MCMBridge
{
	// Wrap the complete label, not individual color spans. Offsets stay on UTF-8
	// boundaries and colors are reapplied only after the line breaks are known.
	template <class Measure>
	std::vector<std::vector<SkyUIRichTextSpan>> WrapRichText(const SkyUIRichText& a_text, float a_width, Measure a_measure)
	{
		std::string text;
		for (const auto& span : a_text.spans)
			text += span.text;
		std::vector<std::vector<SkyUIRichTextSpan>> lines;
		std::size_t                                 start{};
		while (start < text.size()) {
			const auto newline = text.find('\n', start);
			const auto limit = newline == std::string::npos ? text.size() : newline;
			auto       end = start;
			auto       space = std::string::npos;
			while (end < limit) {
				auto next = end + 1;
				while (next < limit && (static_cast<unsigned char>(text[next]) & 0xC0U) == 0x80U)
					++next;
				if (end > start && a_measure(std::string_view(text).substr(start, next - start)) > a_width)
					break;
				if (text[end] == ' ' || text[end] == '\t')
					space = end;
				end = next;
			}
			auto nextStart = end;
			if (end < limit && text[end] != ' ' && text[end] != '\t' && space != std::string::npos && space > start) {
				end = space;
				nextStart = space + 1;
			}
			lines.emplace_back();
			std::size_t offset{};
			for (const auto& span : a_text.spans) {
				const auto first = (std::max)(start, offset);
				const auto last = (std::min)(end, offset + span.text.size());
				if (first < last)
					lines.back().push_back({ span.text.substr(first - offset, last - first), span.color });
				offset += span.text.size();
			}
			if (end == limit) {
				start = limit + (newline != std::string::npos ? 1 : 0);
			} else {
				start = nextStart;
				while (start < limit && (text[start] == ' ' || text[start] == '\t'))
					++start;
			}
		}
		if (text.empty() || text.back() == '\n')
			lines.emplace_back();
		return lines;
	}
}
