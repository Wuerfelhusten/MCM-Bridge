#include "MCMBridge/Core/SkyUIRichText.h"

#include <algorithm>
#include <array>

namespace
{
	char LowerASCII(char a_value)
	{
		return a_value >= 'A' && a_value <= 'Z' ? static_cast<char>(a_value - 'A' + 'a') : a_value;
	}

	std::string_view Trim(std::string_view a_value)
	{
		while (!a_value.empty() && (a_value.front() == ' ' || a_value.front() == '\t')) {
			a_value.remove_prefix(1);
		}
		while (!a_value.empty() && (a_value.back() == ' ' || a_value.back() == '\t' || a_value.back() == '/')) {
			a_value.remove_suffix(1);
		}
		return a_value;
	}

	std::string TagName(std::string_view a_tag, bool& a_closing)
	{
		a_tag = Trim(a_tag);
		a_closing = !a_tag.empty() && a_tag.front() == '/';
		if (a_closing) {
			a_tag.remove_prefix(1);
			a_tag = Trim(a_tag);
		}
		std::string name;
		for (const auto character : a_tag) {
			if ((character < 'A' || character > 'Z') && (character < 'a' || character > 'z')) {
				break;
			}
			name.push_back(LowerASCII(character));
		}
		return name;
	}

	std::optional<std::uint32_t> ParseColor(std::string_view a_tag)
	{
		std::string lower(a_tag);
		std::ranges::transform(lower, lower.begin(), LowerASCII);
		auto position = lower.find("color");
		if (position == std::string::npos) {
			return std::nullopt;
		}
		position += 5;
		while (position < a_tag.size() && (a_tag[position] == ' ' || a_tag[position] == '\t')) {
			++position;
		}
		if (position >= a_tag.size() || a_tag[position++] != '=') {
			return std::nullopt;
		}
		while (position < a_tag.size() && (a_tag[position] == ' ' || a_tag[position] == '\t')) {
			++position;
		}
		char quote{};
		if (position < a_tag.size() && (a_tag[position] == '\'' || a_tag[position] == '"')) {
			quote = a_tag[position++];
		}
		if (position < a_tag.size() && a_tag[position] == '#') {
			++position;
		}
		if (position + 6 > a_tag.size()) {
			return std::nullopt;
		}
		std::uint32_t color{};
		for (std::size_t index = 0; index < 6; ++index) {
			const auto digit = LowerASCII(a_tag[position + index]);
			const auto value = digit >= '0' && digit <= '9' ? digit - '0' :
			                   digit >= 'a' && digit <= 'f' ? digit - 'a' + 10 :
			                                                  -1;
			if (value < 0) {
				return std::nullopt;
			}
			color = color * 16U + static_cast<std::uint32_t>(value);
		}
		position += 6;
		if (quote && (position >= a_tag.size() || a_tag[position] != quote)) {
			return std::nullopt;
		}
		return color;
	}

	std::optional<std::pair<std::string_view, char>> Entity(std::string_view a_source)
	{
		using Pair = std::pair<std::string_view, char>;
		constexpr std::array entities{
			Pair{ "&amp;", '&' },
			Pair{ "&lt;", '<' },
			Pair{ "&gt;", '>' },
			Pair{ "&quot;", '"' },
			Pair{ "&apos;", '\'' }
		};
		const auto found = std::ranges::find_if(entities, [&](const auto& a_entity) {
			return a_source.starts_with(a_entity.first);
		});
		return found != entities.end() ? std::optional(*found) : std::nullopt;
	}
}

namespace MCMBridge
{
	SkyUIRichText ParseSkyUIRichText(std::string_view a_source)
	{
		SkyUIRichText                             result;
		std::vector<std::optional<std::uint32_t>> colors{ std::nullopt };
		std::string                               buffer;
		const auto                                flush = [&] {
			if (buffer.empty()) {
				return;
			}
			result.plainText += buffer;
			if (!result.spans.empty() && result.spans.back().color == colors.back()) {
				result.spans.back().text += buffer;
			} else {
				result.spans.push_back({ buffer, colors.back() });
			}
			buffer.clear();
		};

		for (std::size_t index = 0; index < a_source.size();) {
			if (a_source[index] == '<') {
				const auto end = a_source.find('>', index + 1);
				if (end != std::string_view::npos && end - index <= 256) {
					const auto tag = a_source.substr(index + 1, end - index - 1);
					bool       closing{};
					const auto name = TagName(tag, closing);
					if (!name.empty()) {
						if (name == "font") {
							flush();
							if (closing) {
								if (colors.size() > 1) {
									colors.pop_back();
								}
							} else {
								const auto color = ParseColor(tag);
								colors.push_back(color ? color : colors.back());
							}
						} else if (name == "br") {
							buffer.push_back('\n');
						}
						index = end + 1;
						continue;
					}
				}
			}
			if (a_source[index] == '&') {
				if (const auto entity = Entity(a_source.substr(index))) {
					buffer.push_back(entity->second);
					index += entity->first.size();
					continue;
				}
			}
			buffer.push_back(a_source[index++]);
		}
		flush();
		return result;
	}

	std::string PlainSkyUIText(std::string_view a_source)
	{
		return ParseSkyUIRichText(a_source).plainText;
	}
}
