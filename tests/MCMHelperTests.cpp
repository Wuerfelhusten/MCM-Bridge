#include "MCMBridge/Core/MCMHelperMerge.h"
#include "MCMBridge/Core/MCMHelperParser.h"
#include "MCMBridge/Core/MCMHelperValidation.h"

#include <catch2/catch_test_macros.hpp>

#include <nlohmann/json.hpp>
#include <stdexcept>

namespace
{
	const MCMBridge::MCMControl& Control(const MCMBridge::MCMMod& a_mod, std::string_view a_explicitID)
	{
		for (const auto& page : a_mod.pages) {
			for (const auto& control : page.controls) {
				if (control.identity.explicitID == a_explicitID)
					return control;
			}
		}
		throw std::runtime_error("Fixture control was not found");
	}
}

TEST_CASE("MCM Helper parser applies user settings over defaults")
{
	const auto                root = std::filesystem::path(MCM_BRIDGE_FIXTURE_DIR) / "MCMHelper" / "MCM";
	MCMBridge::MCMHelperPaths paths{
		.config = root / "Config" / "MCMBridgeFixture" / "config.json",
		.defaults = root / "Config" / "MCMBridgeFixture" / "settings.ini",
		.userSettings = root / "Settings" / "MCMBridgeFixture.ini"
	};

	auto mod = MCMBridge::MCMHelperParser{}.Parse(paths);
	REQUIRE(mod);
	REQUIRE(mod->pages.size() == 1);
	REQUIRE(mod->pages.front().controls.size() == 11);
	const auto& enabled = Control(*mod, "bEnabled:General");
	CHECK(std::get<bool>(enabled.value) == false);
	CHECK(std::get<bool>(*enabled.defaultValue) == true);
	CHECK(enabled.groupControl == 1);
	const auto& amount = Control(*mod, "fAmount:General");
	CHECK(std::get<float>(amount.value) == 7.0F);
	CHECK(std::get<float>(*amount.defaultValue) == 5.0F);
	REQUIRE(amount.condition);
	CHECK(amount.condition->expression == "1");
	CHECK(amount.condition->behavior == "disable");
	CHECK(Control(*mod, "iMode:General").menu->options.size() == 3);
	const auto& dynamic = Control(*mod, "sDynamicMode:General");
	CHECK(dynamic.menu->availability == MCMBridge::MetadataAvailability::kDynamic);
	CHECK(dynamic.writeCapability == MCMBridge::WriteCapability::kMissingOptions);
	CHECK(dynamic.source.formID == 0x1234U);
	REQUIRE(dynamic.action);
	CHECK(dynamic.action->form == "Fixture.esp|1234");
	CHECK(dynamic.action->parameters.size() == 3);
	const auto& stepper = Control(*mod, "iSize:General");
	CHECK(stepper.type == MCMBridge::MCMControlType::kStepper);
	CHECK(stepper.menu->options.size() == 3);
	CHECK(std::get<std::int32_t>(stepper.value) == 1);
	CHECK(std::get<std::int32_t>(Control(*mod, "iHotkey:General").value) == 57);
	const auto& color = Control(*mod, "rTint:General");
	CHECK(color.type == MCMBridge::MCMControlType::kColor);
	CHECK(std::get<std::uint32_t>(color.value) == 0x00AABBCCU);
	CHECK(std::get<std::uint32_t>(*color.defaultValue) == 0x00112233U);
	const auto& input = Control(*mod, "sName:General");
	CHECK(input.type == MCMBridge::MCMControlType::kInput);
	CHECK(input.source.kind == MCMBridge::ValueSourceKind::kModSetting);
	CHECK(std::get<std::string>(input.value) == "Player Name");
	const auto& text = Control(*mod, "sStatus:General");
	CHECK(text.source.kind == MCMBridge::ValueSourceKind::kProperty);
	REQUIRE(text.action);
	CHECK(text.action->scriptName == "FixtureActions");
}

TEST_CASE("MCM Helper metadata merges only unique live controls")
{
	MCMBridge::MCMMod live;
	live.backend = MCMBridge::MCMBackendKind::kMCMHelper;
	live.ownerPlugin = "Fixture.esp";
	live.questFormID = 0x1234;
	live.scriptName = "FixtureConfig";
	MCMBridge::MCMPage page;
	page.index = 0;
	page.rawName = "General";
	MCMBridge::MCMControl control;
	control.type = MCMBridge::MCMControlType::kMenu;
	control.label = "Mode";
	control.menu = MCMBridge::MenuMetadata{ .selectedIndex = 1 };
	page.controls.push_back(control);
	live.pages.push_back(page);

	MCMBridge::MCMMod  parsed;
	MCMBridge::MCMPage parsedPage;
	parsedPage.index = 0;
	MCMBridge::MCMControl metadata = control;
	metadata.identity.stableID = "helper-control:test";
	metadata.identity.explicitID = "iMode:General";
	metadata.identity.confidence = MCMBridge::IdentityConfidence::kHigh;
	metadata.menu = MCMBridge::MenuMetadata{ .options = { "One", "Two" }, .availability = MCMBridge::MetadataAvailability::kAvailable };
	parsedPage.controls.push_back(metadata);
	parsed.pages.push_back(parsedPage);

	MCMBridge::MergeMCMHelperMetadata(live, parsed);
	CHECK(live.pages.front().controls.front().identity.stableID.starts_with("helper-control:"));
	CHECK(live.pages.front().controls.front().identity.explicitID == "iMode:General");
	CHECK(live.pages.front().controls.front().menu->selectedIndex == 1);
	CHECK(live.pages.front().controls.front().writeCapability == MCMBridge::WriteCapability::kWritable);
}

TEST_CASE("MCM Helper parser rejects an invalid schema root")
{
	MCMBridge::MCMHelperPaths paths{
		.config = std::filesystem::path(MCM_BRIDGE_FIXTURE_DIR) / "MCMHelper" / "invalid-config.json"
	};
	auto result = MCMBridge::MCMHelperParser{}.Parse(paths);
	REQUIRE_FALSE(result);
	CHECK(result.error().code == MCMBridge::BridgeErrorCode::kInvalidData);
}

TEST_CASE("MCM Helper validation enforces requirements and control types")
{
	auto document = nlohmann::json::parse(R"({
		"modName": "Fixture",
		"displayName": "Fixture",
		"minMcmVersion": 13,
		"pluginRequirements": ["Fixture.esp"],
		"content": [{"type": "toggle", "ignoreConflicts": false}]
	})");
	CHECK(MCMBridge::ValidateMCMHelperConfig(document));
	document["minMcmVersion"] = 12;
	CHECK(MCMBridge::ValidateMCMHelperConfig(document));
	document["minMcmVersion"] = 13;
	document["content"][0]["type"] = "unsupported";
	CHECK_FALSE(MCMBridge::ValidateMCMHelperConfig(document));
}

TEST_CASE("MCM Helper parser preserves custom content pages")
{
	MCMBridge::MCMHelperPaths paths{
		.config = std::filesystem::path(MCM_BRIDGE_FIXTURE_DIR) / "MCMHelper" / "custom-config.json"
	};
	auto result = MCMBridge::MCMHelperParser{}.Parse(paths);
	REQUIRE(result);
	REQUIRE(result->pages.size() == 2);
	const auto& mainPage = result->pages[0];
	REQUIRE(mainPage.customContent);
	CHECK(mainPage.index == -1);
	CHECK(mainPage.displayName == "Original");
	CHECK(mainPage.customContent->source == "fixture/main.swf");
	CHECK(mainPage.customContent->x == 12.0F);
	CHECK(mainPage.customContent->y == 34.5F);
	const auto& subPage = result->pages[1];
	REQUIRE(subPage.customContent);
	CHECK(subPage.index == 0);
	CHECK(subPage.rawName == "Custom Page");
	CHECK(subPage.customContent->source == "fixture/page.swf");
}

TEST_CASE("MCM Helper merge marks and adds custom content pages")
{
	MCMBridge::MCMMod  live;
	MCMBridge::MCMPage livePage;
	livePage.index = 0;
	livePage.rawName = "Custom Page";
	live.pages.push_back(livePage);

	MCMBridge::MCMMod  parsed;
	MCMBridge::MCMPage mainPage;
	mainPage.index = -1;
	mainPage.stableID = "main";
	mainPage.customContent = MCMBridge::CustomContentMetadata{ .source = "main.swf" };
	parsed.pages.push_back(mainPage);
	MCMBridge::MCMPage subPage;
	subPage.index = 0;
	subPage.rawName = "Custom Page";
	subPage.customContent = MCMBridge::CustomContentMetadata{ .source = "page.swf" };
	parsed.pages.push_back(subPage);

	MCMBridge::MergeMCMHelperMetadata(live, parsed);
	REQUIRE(live.pages.size() == 2);
	REQUIRE(live.pages[0].customContent);
	CHECK(live.pages[0].customContent->source == "page.swf");
	REQUIRE(live.pages[1].customContent);
	CHECK(live.pages[1].index == -1);
}

TEST_CASE("MCM Helper stepper metadata promotes a live text control")
{
	MCMBridge::MCMMod live;
	live.ownerPlugin = "Fixture.esp";
	live.questFormID = 0x1234;
	live.scriptName = "FixtureConfig";
	MCMBridge::MCMPage livePage;
	livePage.index = 0;
	livePage.rawName = "General";
	MCMBridge::MCMControl liveControl;
	liveControl.type = MCMBridge::MCMControlType::kText;
	liveControl.value = std::string("Medium");
	liveControl.identity.optionIndex = 2;
	livePage.controls.push_back(liveControl);
	live.pages.push_back(livePage);

	MCMBridge::MCMMod  parsed;
	MCMBridge::MCMPage parsedPage;
	parsedPage.index = 0;
	MCMBridge::MCMControl stepper;
	stepper.type = MCMBridge::MCMControlType::kStepper;
	stepper.identity.optionIndex = 2;
	stepper.identity.explicitID = "iSize:General";
	stepper.menu = MCMBridge::MenuMetadata{
		.options = { "Small", "Medium", "Large" },
		.availability = MCMBridge::MetadataAvailability::kAvailable
	};
	parsedPage.controls.push_back(stepper);
	parsed.pages.push_back(parsedPage);

	MCMBridge::MergeMCMHelperMetadata(live, parsed);
	const auto& merged = live.pages.front().controls.front();
	CHECK(merged.type == MCMBridge::MCMControlType::kStepper);
	CHECK(merged.menu->options.size() == 3);
	CHECK(merged.menu->selectedIndex == 1);
	CHECK(std::get<std::int32_t>(merged.value) == 1);
	CHECK(merged.writeCapability == MCMBridge::WriteCapability::kWritable);
}

TEST_CASE("MCM Helper merge does not shift metadata after a skipped control")
{
	MCMBridge::MCMMod live;
	live.ownerPlugin = "Fixture.esp";
	live.questFormID = 0x1234;
	live.scriptName = "FixtureConfig";
	MCMBridge::MCMPage livePage;
	livePage.index = 0;
	MCMBridge::MCMControl liveToggle;
	liveToggle.type = MCMBridge::MCMControlType::kToggle;
	liveToggle.label = "Visible";
	liveToggle.identity.optionIndex = 0;
	livePage.controls.push_back(liveToggle);
	live.pages.push_back(livePage);

	MCMBridge::MCMMod  parsed;
	MCMBridge::MCMPage parsedPage;
	parsedPage.index = 0;
	MCMBridge::MCMControl skipped = liveToggle;
	skipped.label = "Hidden";
	skipped.hidden = true;
	skipped.identity.explicitID = "bHidden:General";
	parsedPage.controls.push_back(skipped);
	MCMBridge::MCMControl visible = liveToggle;
	visible.label = "Visible";
	visible.identity.optionIndex = 2;
	visible.identity.explicitID = "bVisible:General";
	visible.identity.confidence = MCMBridge::IdentityConfidence::kHigh;
	parsedPage.controls.push_back(visible);
	parsed.pages.push_back(parsedPage);

	MCMBridge::MergeMCMHelperMetadata(live, parsed);
	CHECK(live.pages.front().controls.front().identity.explicitID == "bVisible:General");
}

TEST_CASE("Unmatched MCM Helper text remains a display element")
{
	MCMBridge::MCMMod  live;
	MCMBridge::MCMPage livePage;
	livePage.index = 0;
	MCMBridge::MCMControl text;
	text.type = MCMBridge::MCMControlType::kText;
	text.value = std::string("<font color='#dd3333'>Not taken</font>");
	text.writeCapability = MCMBridge::WriteCapability::kWritable;
	livePage.controls.push_back(text);
	live.pages.push_back(livePage);

	MCMBridge::MCMMod  parsed;
	MCMBridge::MCMPage parsedPage;
	parsedPage.index = 0;
	parsed.pages.push_back(parsedPage);

	MCMBridge::MergeMCMHelperMetadata(live, parsed);
	CHECK(live.pages.front().controls.front().writeCapability == MCMBridge::WriteCapability::kReadOnly);
}
