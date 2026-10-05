#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace MCMBridge
{
	struct SkyUIRichTextSpan
	{
		std::string                  text;
		std::optional<std::uint32_t> color;
	};

	struct SkyUIRichText
	{
		std::string                    plainText;
		std::vector<SkyUIRichTextSpan> spans;
	};

	SkyUIRichText ParseSkyUIRichText(std::string_view a_source);
	std::string   PlainSkyUIText(std::string_view a_source);
}
