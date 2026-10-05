#include "MCMBridge/Core/MCMHelperParser.h"

#include "MCMBridge/Core/MCMHelperValidation.h"
#include "MCMBridge/Core/MCMHelperValues.h"
#include "MCMBridge/Core/StableId.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <format>
#include <fstream>

namespace
{
	using Json = nlohmann::json;

	MCMBridge::MCMControlType ParseType(std::string_view a_type)
	{
		using MCMBridge::MCMControlType;
		if (a_type == "empty")
			return MCMControlType::kEmpty;
		if (a_type == "header")
			return MCMControlType::kHeader;
		if (a_type == "text")
			return MCMControlType::kText;
		if (a_type == "toggle" || a_type == "hiddenToggle")
			return MCMControlType::kToggle;
		if (a_type == "slider")
			return MCMControlType::kSlider;
		if (a_type == "menu" || a_type == "enum")
			return MCMControlType::kMenu;
		if (a_type == "stepper")
			return MCMControlType::kStepper;
		if (a_type == "color")
			return MCMControlType::kColor;
		if (a_type == "keymap")
			return MCMControlType::kKeymap;
		if (a_type == "input")
			return MCMControlType::kInput;
		return MCMControlType::kUnknown;
	}

	std::string ReadString(const Json& a_value, std::string_view a_key)
	{
		const auto found = a_value.find(a_key);
		return found != a_value.end() && found->is_string() ? found->get<std::string>() : std::string{};
	}

	std::int32_t ReadInteger(const Json& a_value, std::string_view a_key, std::int32_t a_fallback)
	{
		const auto found = a_value.find(a_key);
		return found != a_value.end() && found->is_number_integer() ? found->get<std::int32_t>() : a_fallback;
	}

	float ReadFloat(const Json& a_value, std::string_view a_key, float a_fallback)
	{
		const auto found = a_value.find(a_key);
		return found != a_value.end() && found->is_number() ? found->get<float>() : a_fallback;
	}

	MCMBridge::MCMValue ParseParameter(const Json& a_value)
	{
		if (a_value.is_boolean())
			return a_value.get<bool>();
		if (a_value.is_number_float())
			return a_value.get<float>();
		if (a_value.is_number_integer())
			return static_cast<std::int32_t>(a_value.get<std::int64_t>());
		if (a_value.is_string())
			return a_value.get<std::string>();
		return std::monostate{};
	}

	std::optional<MCMBridge::ActionMetadata> ParseAction(const Json& a_value)
	{
		if (!a_value.is_object())
			return std::nullopt;
		MCMBridge::ActionMetadata action;
		action.type = ReadString(a_value, "type");
		action.functionName = ReadString(a_value, "function");
		action.form = ReadString(a_value, "form");
		action.scriptName = ReadString(a_value, "scriptName");
		if (action.scriptName.empty())
			action.scriptName = ReadString(a_value, "script");
		action.command = ReadString(a_value, "command");
		if (const auto params = a_value.find("params"); params != a_value.end() && params->is_array()) {
			for (const auto& parameter : *params) action.parameters.push_back(ParseParameter(parameter));
		}
		return action;
	}

	bool IsValueControl(MCMBridge::MCMControlType a_type)
	{
		using MCMBridge::MCMControlType;
		return a_type == MCMControlType::kToggle || a_type == MCMControlType::kSlider ||
		       a_type == MCMControlType::kMenu || a_type == MCMControlType::kStepper ||
		       a_type == MCMControlType::kColor || a_type == MCMControlType::kKeymap ||
		       a_type == MCMControlType::kInput;
	}
}

namespace MCMBridge
{
	Result<MCMMod> MCMHelperParser::Parse(const MCMHelperPaths& a_paths) const
	{
		std::ifstream input(a_paths.config);
		if (!input) {
			return std::unexpected(BridgeError{ BridgeErrorCode::kIoError, "MCM Helper config could not be opened" });
		}
		Json document;
		try {
			input >> document;
		} catch (const Json::exception& error) {
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, error.what() });
		}
		if (auto validation = ValidateMCMHelperConfig(document); !validation) {
			return std::unexpected(validation.error());
		}

		const auto modName = ReadString(document, "modName");
		if (modName.empty()) {
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "MCM Helper config is missing modName" });
		}
		MCMHelperSettings defaults;
		if (a_paths.defaults && std::filesystem::exists(*a_paths.defaults))
			ReadMCMHelperIni(*a_paths.defaults, defaults);
		auto current = defaults;
		if (a_paths.userSettings && std::filesystem::exists(*a_paths.userSettings))
			ReadMCMHelperIni(*a_paths.userSettings, current);

		MCMMod mod;
		mod.stableID = MakeStableID("helper-mod", std::array<std::string_view, 1>{ modName });
		mod.displayName = ReadString(document, "displayName");
		if (mod.displayName.empty())
			mod.displayName = modName;
		mod.backend = MCMBackendKind::kMCMHelper;
		mod.scriptName = "MCM_ConfigBase";
		mod.minimumMCMHelperVersion = document.value("minMcmVersion", std::uint32_t{});
		if (const auto requirements = document.find("pluginRequirements"); requirements != document.end()) {
			for (const auto& requirement : *requirements) {
				mod.pluginRequirements.push_back(requirement.get<std::string>());
			}
		}

		auto parsePage = [&](const Json& a_pageJson, std::int32_t a_pageIndex) -> Result<void> {
			if (!a_pageJson.is_object()) {
				return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "MCM Helper page is not an object" });
			}

			MCMPage page;
			page.rawName = ReadString(a_pageJson, "pageName");
			page.displayName = ReadString(a_pageJson, "pageDisplayName");
			if (page.rawName.empty())
				page.rawName = page.displayName;
			if (page.displayName.empty())
				page.displayName = page.rawName;
			if (page.displayName.empty() && a_pageIndex < 0)
				page.displayName = "Original";
			page.index = a_pageIndex;
			page.stableID = MakeStableID("helper-page", std::array<std::string_view, 2>{ mod.stableID, page.rawName });

			if (const auto custom = a_pageJson.find("customContent"); custom != a_pageJson.end()) {
				if (!custom->is_object()) {
					return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "MCM Helper customContent is not an object" });
				}
				const auto source = ReadString(*custom, "source");
				if (source.empty()) {
					return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "MCM Helper customContent is missing source" });
				}
				page.customContent = CustomContentMetadata{
					.source = source,
					.x = ReadFloat(*custom, "x", 0.0F),
					.y = ReadFloat(*custom, "y", 0.0F)
				};
				mod.pages.push_back(std::move(page));
				return {};
			}

			const auto content = a_pageJson.find("content");
			if (content == a_pageJson.end() || !content->is_array()) {
				return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "MCM Helper page is missing content" });
			}

			std::size_t  controlIndex{};
			std::int32_t cursorPosition{};
			const auto   cursorStep = ReadString(a_pageJson, "cursorFillMode") == "topToBottom" ? 2 : 1;
			for (const auto& controlJson : *content) {
				const auto rawType = controlJson.is_object() ? ReadString(controlJson, "type") : std::string{};
				const auto type = ParseType(rawType);
				if (type == MCMControlType::kUnknown) {
					++controlIndex;
					continue;
				}

				MCMControl control;
				const auto explicitID = ReadString(controlJson, "id");
				const auto sourceID = explicitID.empty() ? std::format("control-{}", controlIndex) : explicitID;
				if (const auto position = ReadInteger(controlJson, "position", -1); position >= 0)
					cursorPosition = position;
				control.identity = SettingIdentity{
					.stableID = MakeHelperControlID(modName, 0, mod.scriptName, page.rawName, {}, sourceID),
					.backend = MCMBackendKind::kMCMHelper,
					.scriptName = mod.scriptName,
					.pageKey = page.rawName,
					.explicitID = sourceID,
					.pageIndex = page.index,
					.optionIndex = static_cast<std::uint16_t>(cursorPosition),
					.confidence = explicitID.empty() ? IdentityConfidence::kLow : IdentityConfidence::kHigh
				};
				control.type = type;
				control.label = ReadString(controlJson, "text");
				control.help = ReadString(controlJson, "help");
				control.hidden = rawType == "hiddenToggle";
				control.groupControl = static_cast<std::uint32_t>((std::max)(0, ReadInteger(controlJson, "groupControl", 0)));
				control.ignoreConflicts = controlJson.value("ignoreConflicts", false);
				control.layout = { cursorPosition, cursorPosition % 2 };
				control.source.settingID = explicitID;
				if (const auto options = controlJson.find("valueOptions"); options != controlJson.end()) {
					ApplyMCMHelperValueOptions(control, *options, defaults, current);
				}
				if (const auto action = controlJson.find("action"); action != controlJson.end())
					control.action = ParseAction(*action);
				if (const auto condition = controlJson.find("groupCondition"); condition != controlJson.end()) {
					control.condition = ConditionMetadata{ condition->dump(), ReadString(controlJson, "groupBehavior") };
					if (control.condition->behavior.empty())
						control.condition->behavior = "disable";
				}

				const auto hasSource = control.source.kind != ValueSourceKind::kNone;
				control.writeCapability = (IsValueControl(type) && hasSource) || (type == MCMControlType::kText && control.action) ?
				                              WriteCapability::kWritable :
				                              WriteCapability::kReadOnly;
				if ((type == MCMControlType::kMenu || type == MCMControlType::kStepper) &&
					control.menu && control.menu->options.empty()) {
					control.writeCapability = WriteCapability::kMissingOptions;
				}
				page.controls.push_back(std::move(control));
				++controlIndex;
				cursorPosition += cursorStep;
			}
			mod.pages.push_back(std::move(page));
			return {};
		};

		bool parsedAny{};
		if (document.contains("content") || document.contains("customContent")) {
			if (auto result = parsePage(document, -1); !result)
				return std::unexpected(result.error());
			parsedAny = true;
		}
		if (const auto pages = document.find("pages"); pages != document.end() && pages->is_array()) {
			std::int32_t pageIndex{};
			for (const auto& page : *pages) {
				if (auto result = parsePage(page, pageIndex++); !result)
					return std::unexpected(result.error());
			}
			parsedAny = true;
		}
		if (!parsedAny) {
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "MCM Helper config has no pages or content" });
		}
		return mod;
	}
}
