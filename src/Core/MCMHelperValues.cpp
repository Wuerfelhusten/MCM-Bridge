#include "MCMBridge/Core/MCMHelperValues.h"

#include <SimpleIni.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <format>
#include <sstream>

namespace
{
	using Json = nlohmann::json;

	std::string ReadString(const Json& a_value, std::string_view a_key)
	{
		const auto found = a_value.find(a_key);
		return found != a_value.end() && found->is_string() ? found->get<std::string>() : std::string{};
	}

	float ReadFloat(const Json& a_value, std::string_view a_key, float a_fallback)
	{
		const auto found = a_value.find(a_key);
		return found != a_value.end() && found->is_number() ? found->get<float>() : a_fallback;
	}

	MCMBridge::ValueSourceKind SourceKind(std::string_view a_sourceType)
	{
		using MCMBridge::ValueSourceKind;
		if (a_sourceType.starts_with("ModSetting")) {
			return ValueSourceKind::kModSetting;
		}
		if (a_sourceType.starts_with("PropertyValue")) {
			return ValueSourceKind::kProperty;
		}
		if (a_sourceType == "GlobalValue") {
			return ValueSourceKind::kGlobal;
		}
		return a_sourceType.empty() ? ValueSourceKind::kNone : ValueSourceKind::kDerived;
	}

	std::uint32_t ParseFormID(std::string_view a_sourceForm)
	{
		const auto separator = a_sourceForm.find('|');
		if (separator == std::string_view::npos || separator + 1 >= a_sourceForm.size())
			return 0U;
		auto value = a_sourceForm.substr(separator + 1);
		if (value.starts_with("0x") || value.starts_with("0X"))
			value.remove_prefix(2);
		std::uint32_t formID{};
		const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), formID, 16);
		return error == std::errc{} && end == value.data() + value.size() ? formID : 0U;
	}

	std::uint32_t ParseColor(std::string_view a_value)
	{
		std::istringstream input{ std::string(a_value) };
		std::uint32_t      red{};
		std::uint32_t      green{};
		std::uint32_t      blue{};
		char               separator{};
		if (input >> red >> separator >> green >> separator >> blue) {
			return ((red & 0xFFU) << 16) | ((green & 0xFFU) << 8) | (blue & 0xFFU);
		}
		return 0U;
	}

	MCMBridge::MCMValue ParseIniValue(std::string_view a_key, const char* a_rawValue)
	{
		if (!a_rawValue || a_key.empty()) {
			return std::monostate{};
		}
		try {
			switch (a_key.front()) {
			case 'b':
				return std::string_view(a_rawValue) == "1" || std::string_view(a_rawValue) == "true";
			case 'i':
				return static_cast<std::int32_t>(std::stol(a_rawValue));
			case 'u':
				return static_cast<std::uint32_t>(std::stoul(a_rawValue));
			case 'f':
				return std::stof(a_rawValue);
			case 'r':
				return ParseColor(a_rawValue);
			case 's':
				return std::string(a_rawValue);
			default:
				return std::monostate{};
			}
		} catch (const std::exception&) {
			return std::monostate{};
		}
	}

	MCMBridge::MCMValue ParseDefault(const Json& a_value, const MCMBridge::MCMControl& a_control)
	{
		if (a_value.is_boolean()) {
			return a_value.get<bool>();
		}
		if (a_value.is_number_float()) {
			return a_value.get<float>();
		}
		if (a_value.is_number_unsigned() ||
			(a_control.type == MCMBridge::MCMControlType::kColor && a_value.is_number_integer())) {
			return static_cast<std::uint32_t>(a_value.get<std::uint64_t>());
		}
		if (a_value.is_number_integer()) {
			return static_cast<std::int32_t>(a_value.get<std::int64_t>());
		}
		if (!a_value.is_string()) {
			return std::monostate{};
		}

		const auto text = a_value.get<std::string>();
		if (text.starts_with("{s}")) {
			return text.substr(3);
		}
		if (text.starts_with("{r}")) {
			return ParseColor(std::string_view(text).substr(3));
		}
		try {
			if (text.starts_with("{b}")) {
				return text.substr(3) == "1" || text.substr(3) == "true";
			}
			if (text.starts_with("{f}")) {
				return std::stof(text.substr(3));
			}
			if (text.starts_with("{u}")) {
				return static_cast<std::uint32_t>(std::stoul(text.substr(3)));
			}
			if (text.starts_with("{i}")) {
				return static_cast<std::int32_t>(std::stol(text.substr(3)));
			}
		} catch (const std::exception&) {
			return std::monostate{};
		}
		return text;
	}

	void ReadStringArray(const Json& a_source, std::string_view a_key, std::vector<std::string>& a_target)
	{
		const auto values = a_source.find(a_key);
		if (values == a_source.end() || !values->is_array()) {
			return;
		}
		for (const auto& value : *values) {
			if (value.is_string()) {
				a_target.push_back(value.get<std::string>());
			}
		}
	}
}

namespace MCMBridge
{
	void ReadMCMHelperIni(const std::filesystem::path& a_path, MCMHelperSettings& a_settings)
	{
		CSimpleIniA ini;
		ini.SetUnicode();
		if (ini.LoadFile(a_path.string().c_str()) < 0) {
			return;
		}
		CSimpleIniA::TNamesDepend sections;
		ini.GetAllSections(sections);
		for (const auto& section : sections) {
			CSimpleIniA::TNamesDepend keys;
			ini.GetAllKeys(section.pItem, keys);
			for (const auto& key : keys) {
				const auto settingID = std::format("{}:{}", key.pItem, section.pItem);
				a_settings[settingID] = ParseIniValue(key.pItem, ini.GetValue(section.pItem, key.pItem));
			}
		}
	}

	void ApplyMCMHelperValueOptions(
		MCMControl&              a_control,
		const Json&              a_valueOptions,
		const MCMHelperSettings& a_defaults,
		const MCMHelperSettings& a_current)
	{
		if (!a_valueOptions.is_object()) {
			return;
		}
		a_control.source.sourceType = ReadString(a_valueOptions, "sourceType");
		a_control.source.sourceForm = ReadString(a_valueOptions, "sourceForm");
		a_control.source.formID = ParseFormID(a_control.source.sourceForm);
		a_control.source.scriptName = ReadString(a_valueOptions, "scriptName");
		a_control.source.propertyName = ReadString(a_valueOptions, "propertyName");
		if (a_control.source.sourceType.empty()) {
			a_control.source.sourceType = !a_control.source.propertyName.empty() ? "PropertyValueString" :
			                              !a_control.source.settingID.empty()    ? "ModSettingString" :
			                                                                       std::string{};
		}
		a_control.source.kind = SourceKind(a_control.source.sourceType);
		if (a_control.type == MCMControlType::kText) {
			if (const auto value = a_valueOptions.find("value"); value != a_valueOptions.end() && value->is_string()) {
				a_control.value = value->get<std::string>();
			}
		}

		if (const auto configuredDefault = a_valueOptions.find("defaultValue"); configuredDefault != a_valueOptions.end()) {
			a_control.defaultValue = ParseDefault(*configuredDefault, a_control);
		} else if (const auto found = a_defaults.find(a_control.source.settingID); found != a_defaults.end()) {
			a_control.defaultValue = found->second;
		}
		if (a_control.source.kind == ValueSourceKind::kModSetting) {
			if (const auto found = a_current.find(a_control.source.settingID); found != a_current.end()) {
				a_control.value = found->second;
			}
		}

		if (a_control.type == MCMControlType::kSlider) {
			SliderMetadata slider;
			slider.minimum = ReadFloat(a_valueOptions, "min", 0.0F);
			slider.maximum = ReadFloat(a_valueOptions, "max", 100.0F);
			slider.step = ReadFloat(a_valueOptions, "step", 1.0F);
			slider.format = ReadString(a_valueOptions, "formatString");
			if (slider.format.empty()) {
				slider.format = "{0}";
			}
			if (const auto* value = a_control.defaultValue ? std::get_if<float>(std::addressof(*a_control.defaultValue)) : nullptr) {
				slider.defaultValue = *value;
			}
			slider.availability = MetadataAvailability::kAvailable;
			a_control.slider = std::move(slider);
		}
		if (a_control.type == MCMControlType::kMenu || a_control.type == MCMControlType::kStepper) {
			MenuMetadata menu;
			ReadStringArray(a_valueOptions, "options", menu.options);
			ReadStringArray(a_valueOptions, "shortNames", menu.shortNames);
			menu.availability = menu.options.empty() ? MetadataAvailability::kDynamic : MetadataAvailability::kAvailable;
			if (const auto* index = std::get_if<std::int32_t>(std::addressof(a_control.value))) {
				menu.selectedIndex = *index;
			} else if (const auto* text = std::get_if<std::string>(std::addressof(a_control.value))) {
				const auto found = std::ranges::find(menu.options, *text);
				menu.selectedIndex = found == menu.options.end() ? -1 :
				                                                   static_cast<std::int32_t>(std::distance(menu.options.begin(), found));
			}
			if (const auto* index = a_control.defaultValue ? std::get_if<std::int32_t>(std::addressof(*a_control.defaultValue)) : nullptr) {
				menu.defaultIndex = *index;
			}
			a_control.menu = std::move(menu);
		}
		if (a_control.type == MCMControlType::kColor) {
			ColorMetadata color;
			if (const auto* value = std::get_if<std::uint32_t>(std::addressof(a_control.value))) {
				color.start = *value;
			}
			if (const auto* value = a_control.defaultValue ? std::get_if<std::uint32_t>(std::addressof(*a_control.defaultValue)) : nullptr) {
				color.defaultValue = *value;
			}
			color.availability = MetadataAvailability::kAvailable;
			a_control.color = color;
		}
	}
}
