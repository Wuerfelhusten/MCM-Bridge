#include "MCMBridge/Core/MCMHelperValidation.h"

#include <nlohmann/json.hpp>

#include <array>
#include <cstdint>
#include <format>
#include <limits>

namespace
{
	using Json = nlohmann::json;

	MCMBridge::Result<void> Error(std::string a_message)
	{
		return std::unexpected(MCMBridge::BridgeError{ MCMBridge::BridgeErrorCode::kInvalidData, std::move(a_message) });
	}

	bool IsStringArray(const Json& a_value)
	{
		return a_value.is_array() && std::ranges::all_of(a_value, [](const auto& a_item) { return a_item.is_string(); });
	}

	bool IsVersionNumber(const Json& a_value)
	{
		constexpr auto maximum = (std::numeric_limits<std::uint32_t>::max)();
		if (a_value.is_number_unsigned()) {
			return a_value.get<std::uint64_t>() <= maximum;
		}
		if (a_value.is_number_integer()) {
			const auto number = a_value.get<std::int64_t>();
			return number >= 0 && number <= maximum;
		}
		return false;
	}

	MCMBridge::Result<void> ValidateControl(const Json& a_control, std::size_t a_index)
	{
		if (!a_control.is_object()) {
			return Error(std::format("MCM Helper control {} is not an object", a_index));
		}
		const auto           type = a_control.find("type");
		constexpr std::array types{
			"empty", "header", "text", "toggle", "hiddenToggle", "slider", "stepper",
			"menu", "enum", "color", "keymap", "input"
		};
		if (type == a_control.end() || !type->is_string() || !std::ranges::contains(types, type->get<std::string>())) {
			return Error(std::format("MCM Helper control {} has an invalid type", a_index));
		}
		for (const auto key : { "id", "text", "help" }) {
			if (const auto value = a_control.find(key); value != a_control.end() && !value->is_string()) {
				return Error(std::format("MCM Helper control {} field {} is not a string", a_index, key));
			}
		}
		if (const auto position = a_control.find("position"); position != a_control.end() && !position->is_number_integer()) {
			return Error(std::format("MCM Helper control {} position is not an integer", a_index));
		}
		if (const auto behavior = a_control.find("groupBehavior"); behavior != a_control.end() &&
																   (!behavior->is_string() || !std::ranges::contains(
																								  std::array{ "disable", "hide", "skip" }, behavior->get<std::string>()))) {
			return Error(std::format("MCM Helper control {} has an invalid groupBehavior", a_index));
		}
		if (const auto ignore = a_control.find("ignoreConflicts"); ignore != a_control.end() && !ignore->is_boolean()) {
			return Error(std::format("MCM Helper control {} ignoreConflicts is not a boolean", a_index));
		}
		if (const auto options = a_control.find("valueOptions"); options != a_control.end()) {
			if (!options->is_object()) {
				return Error(std::format("MCM Helper control {} valueOptions is not an object", a_index));
			}
			for (const auto key : { "options", "shortNames" }) {
				if (const auto values = options->find(key); values != options->end() && !IsStringArray(*values)) {
					return Error(std::format("MCM Helper control {} {} is not a string array", a_index, key));
				}
			}
			for (const auto key : { "min", "max", "step" }) {
				if (const auto number = options->find(key); number != options->end() && !number->is_number()) {
					return Error(std::format("MCM Helper control {} {} is not numeric", a_index, key));
				}
			}
		}
		return {};
	}

	MCMBridge::Result<void> ValidatePage(const Json& a_page, std::string_view a_location, bool a_requireName)
	{
		if (!a_page.is_object()) {
			return Error(std::format("MCM Helper {} is not an object", a_location));
		}
		if (a_requireName) {
			const auto name = a_page.find("pageDisplayName");
			if (name == a_page.end() || !name->is_string()) {
				return Error(std::format("MCM Helper {} is missing pageDisplayName", a_location));
			}
		}
		if (const auto fill = a_page.find("cursorFillMode"); fill != a_page.end() &&
															 (!fill->is_string() || !std::ranges::contains(
																						std::array{ "leftToRight", "topToBottom" }, fill->get<std::string>()))) {
			return Error(std::format("MCM Helper {} has an invalid cursorFillMode", a_location));
		}
		const auto content = a_page.find("content");
		const auto custom = a_page.find("customContent");
		if ((content == a_page.end()) == (custom == a_page.end())) {
			return Error(std::format("MCM Helper {} must define either content or customContent", a_location));
		}
		if (custom != a_page.end()) {
			const auto source = custom->is_object() ? custom->find("source") : custom->end();
			if (!custom->is_object() || source == custom->end() || !source->is_string()) {
				return Error(std::format("MCM Helper {} has invalid customContent", a_location));
			}
			return {};
		}
		if (!content->is_array()) {
			return Error(std::format("MCM Helper {} content is not an array", a_location));
		}
		for (std::size_t index = 0; index < content->size(); ++index) {
			if (auto result = ValidateControl((*content)[index], index); !result) {
				return result;
			}
		}
		return {};
	}
}

namespace MCMBridge
{
	Result<void> ValidateMCMHelperConfig(const nlohmann::json& a_document)
	{
		if (!a_document.is_object()) {
			return Error("MCM Helper config root is not an object");
		}
		for (const auto key : { "modName", "displayName" }) {
			const auto value = a_document.find(key);
			if (value == a_document.end() || !value->is_string() || value->get_ref<const std::string&>().empty()) {
				return Error(std::format("MCM Helper config is missing {}", key));
			}
		}
		// MCM Helper accepts older version codes and enforces the runtime requirement itself.
		if (const auto version = a_document.find("minMcmVersion"); version != a_document.end() && !IsVersionNumber(*version)) {
			return Error("MCM Helper minMcmVersion must be an unsigned 32-bit integer");
		}
		if (const auto requirements = a_document.find("pluginRequirements"); requirements != a_document.end() &&
																			 !IsStringArray(*requirements)) {
			return Error("MCM Helper pluginRequirements is not a string array");
		}
		if (const auto pages = a_document.find("pages"); pages != a_document.end()) {
			if (!pages->is_array()) {
				return Error("MCM Helper pages is not an array");
			}
			for (std::size_t index = 0; index < pages->size(); ++index) {
				if (auto result = ValidatePage((*pages)[index], std::format("page {}", index), true); !result) {
					return result;
				}
			}
		}
		if (a_document.contains("content") || a_document.contains("customContent")) {
			return ValidatePage(a_document, "root page", false);
		}
		if (!a_document.contains("pages")) {
			return Error("MCM Helper config has no pages or content");
		}
		return {};
	}
}
