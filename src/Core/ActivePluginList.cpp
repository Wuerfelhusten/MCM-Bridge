#include "MCMBridge/Core/ActivePluginList.h"

#include <algorithm>
#include <optional>

namespace
{
	std::string_view Trim(std::string_view a_text)
	{
		const auto first = a_text.find_first_not_of(" \t\r");
		return first == std::string_view::npos ? std::string_view{} :
		                                         a_text.substr(first, a_text.find_last_not_of(" \t\r") - first + 1);
	}

	bool EqualName(std::string_view a_left, std::string_view a_right)
	{
		return std::ranges::equal(a_left, a_right, [](unsigned char a_a, unsigned char a_b) {
			const auto lower = [](unsigned char a_c) { return a_c >= 'A' && a_c <= 'Z' ? a_c + ('a' - 'A') : a_c; };
			return lower(a_a) == lower(a_b);
		});
	}
}

namespace MCMBridge
{
	Result<bool> IsPluginActive(std::istream& a_input, std::string_view a_plugin)
	{
		std::string content;
		char        character{};
		while (a_input.get(character)) {
			if (character == '\0' || content.size() >= 4 * 1024 * 1024) {
				return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Unsupported or oversized plugins.txt" });
			}
			content.push_back(character);
		}
		if (a_input.bad() || !a_input.eof()) {
			return std::unexpected(BridgeError{ BridgeErrorCode::kIoError, "Could not read plugins.txt completely" });
		}
		std::string_view remaining = content;
		if (remaining.starts_with("\xFF\xFE") || remaining.starts_with("\xFE\xFF")) {
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Unsupported plugins.txt encoding" });
		}
		if (remaining.starts_with("\xEF\xBB\xBF"))
			remaining.remove_prefix(3);
		std::optional<bool> state;
		while (!remaining.empty()) {
			const auto end = remaining.find_first_of("\r\n");
			auto       line = Trim(remaining.substr(0, end));
			remaining = end == std::string_view::npos ? std::string_view{} : remaining.substr(end + 1);
			if (line.empty() || line.starts_with('#'))
				continue;
			const bool active = line.starts_with('*');
			if (active)
				line = Trim(line.substr(1));
			if (!EqualName(line, a_plugin))
				continue;
			if (state && *state != active) {
				return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Conflicting plugin activation entries in plugins.txt" });
			}
			state = active;
		}
		return state.value_or(false);
	}

	Result<std::string_view> PluginListFolder(std::uint32_t a_runtime)
	{
		const auto edition = a_runtime & 0xFU;
		const auto patch = (a_runtime >> 4U) & 0xFFFU;
		if (edition == 1 || (edition == 0 && (patch == 659 || patch == 1179)))
			return "Skyrim Special Edition GOG";
		if (edition == 2 || (edition == 0 && patch == 678))
			return "Skyrim Special Edition EPIC";
		if (edition == 0)
			return "Skyrim Special Edition";
		return std::unexpected(BridgeError{ BridgeErrorCode::kUnsupported, "Unknown Skyrim runtime edition" });
	}
}
