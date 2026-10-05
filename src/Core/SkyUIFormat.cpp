#include "MCMBridge/Core/SkyUIFormat.h"

#include <charconv>
#include <format>

namespace
{
	std::string EscapePercents(std::string_view a_value)
	{
		std::string escaped;
		escaped.reserve(a_value.size());
		for (const auto character : a_value) {
			escaped.push_back(character);
			if (character == '%') {
				escaped.push_back('%');
			}
		}
		return escaped;
	}
}

namespace MCMBridge
{
	std::string MakeSliderPrintfFormat(std::string_view a_skyUIFormat)
	{
		const auto open = a_skyUIFormat.find('{');
		const auto close = open == std::string_view::npos ? std::string_view::npos : a_skyUIFormat.find('}', open + 1);
		if (open == std::string_view::npos || close == std::string_view::npos) {
			return a_skyUIFormat.contains('%') ? std::string(a_skyUIFormat) : "%.2f";
		}

		int        precision{};
		const auto precisionText = a_skyUIFormat.substr(open + 1, close - open - 1);
		const auto parsed = std::from_chars(precisionText.data(), precisionText.data() + precisionText.size(), precision);
		if (parsed.ec != std::errc{} || parsed.ptr != precisionText.data() + precisionText.size() || precision < 0 || precision > 9) {
			return "%.2f";
		}

		const auto prefix = EscapePercents(a_skyUIFormat.substr(0, open));
		const auto suffix = EscapePercents(a_skyUIFormat.substr(close + 1));
		return std::format("{}%.{}f{}", prefix, precision, suffix);
	}
}
