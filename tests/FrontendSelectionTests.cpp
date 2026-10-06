#include "MCMBridge/Core/ControlRowLayout.h"
#include "MCMBridge/Core/FrontendSelection.h"
#include "MCMBridge/Core/MCMOrganization.h"
#include "MCMBridge/Core/RichTextLayout.h"
#include "MCMBridge/Framework/RenderContext.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Frontend selection uses only available capabilities and defaults to FLICK")
{
	using namespace MCMBridge;
	CHECK(BridgeSettings{}.preferFlick);
	for (const bool prefer : { false, true }) {
		CHECK(SelectFrontend(false, false, prefer) == Frontend::kNone);
		CHECK(SelectFrontend(true, false, prefer) == Frontend::kFlick);
		CHECK(SelectFrontend(false, true, prefer) == Frontend::kMenuFramework);
	}
	CHECK(SelectFrontend(true, true, true) == Frontend::kFlick);
	CHECK(SelectFrontend(true, true, false) == Frontend::kMenuFramework);
}

TEST_CASE("FLICK page labels remain visible without changing raw navigation identities")
{
	using namespace MCMBridge;
	MCMPage page{ .stableID = "default-page", .index = -1 };
	CHECK(FlickPageLabel(page) == "General");
	CHECK(page.rawName.empty());
	CHECK(page.stableID == "default-page");
	page.rawName = "Original page";
	CHECK(FlickPageLabel(page) == "Original page");
	page.displayName = "Translated page";
	CHECK(FlickPageLabel(page) == "Translated page");
	page.displayName = "<font color='#ffffff'>Title</font>";
	CHECK(FlickPageLabel(page) == "Title");
	page.displayName = " \t";
	CHECK(FlickPageLabel(page) == "General");
	CHECK(page.rawName == "Original page");
}

TEST_CASE("FLICK configuration is visible only for the active preferred frontend")
{
	using namespace MCMBridge;
	for (const auto frontend : { Frontend::kNone, Frontend::kMenuFramework, Frontend::kFlick })
		for (const bool preferred : { false, true })
			CHECK(FlickSettingsVisible(frontend, preferred) == (frontend == Frontend::kFlick && preferred));
}

TEST_CASE("FLICK presentation uses flat configurable groups without changing original names")
{
	using namespace MCMBridge;
	BridgeSettings settings;
	CHECK(FlickMCMGroup(settings, "Conduit").empty());
	settings.groupMCMs = true;
	CHECK(FlickMCMGroup(settings, "Conduit") == "MCMs");
	settings.alphabeticMCMs = true;
	CHECK(FlickMCMGroup(settings, "Conduit") == "MCM - A-C");
	CHECK(FlickMCMGroup(settings, "frostfall") == "MCM - D-G");
	CHECK(FlickMCMGroup(settings, "LOTD Checklist") == "MCM - H-L");
	settings.mcmRangeEnds = "FQZ";
	CHECK(FlickMCMGroup(settings, "LOTD Checklist") == "MCM - G-Q");
	SetMCMAlias(settings, "original-id", "Zeta");
	CHECK(FlickMCMGroup(settings, ResolveMCMAlias(settings, "original-id", "LOTD Checklist")) == "MCM - R-Z");
	CHECK(settings.aliases.at("original-id") == "Zeta");
}

TEST_CASE("Render context routes native text and restores nested frontend boundaries")
{
	using namespace MCMBridge;
	CHECK(renderFrontend == Frontend::kMenuFramework);
	const auto draw = +[](const SkyUIRichText&, bool, bool) {};
	{
		RenderContext flick(Frontend::kFlick, draw, true);
		CHECK(renderFrontend == Frontend::kFlick);
		CHECK(nativeRichTextDraw == draw);
		CHECK(hostedWindowDraw);
		{
			RenderContext framework(Frontend::kMenuFramework);
			CHECK(renderFrontend == Frontend::kMenuFramework);
			CHECK_FALSE(nativeRichTextDraw);
			CHECK_FALSE(hostedWindowDraw);
		}
		CHECK(nativeRichTextDraw == draw);
		CHECK(hostedWindowDraw);
	}
	CHECK(renderFrontend == Frontend::kMenuFramework);
	CHECK_FALSE(nativeRichTextDraw);
	CHECK_FALSE(hostedWindowDraw);
}

TEST_CASE("Control rows preserve action space at large fonts and narrow widths")
{
	using namespace MCMBridge;
	const auto inlineRow = FitControlRow(600, 250, 240, 50, 8);
	CHECK_FALSE(inlineRow.stacked);
	CHECK(inlineRow.widgetOffset == 310);
	CHECK(inlineRow.widgetWidth == 240);
	const auto longLabel = FitControlRow(600, 520, 240, 50, 8);
	CHECK(longLabel.stacked);
	CHECK(longLabel.widgetOffset == 0);
	CHECK(longLabel.widgetWidth == 550);
	for (const auto width : { 0.0F, 80.0F, 320.0F, 600.0F, 1200.0F }) {
		const auto layout = FitControlRow(width, 900, 280, 50, 8);
		CHECK(layout.widgetWidth >= 0);
		CHECK(layout.widgetOffset >= 0);
		CHECK(layout.widgetOffset + layout.widgetWidth <= (std::max)(0.0F, width - 50));
	}
}

TEST_CASE("Shortened labels keep UTF-8 boundaries without changing their source")
{
	using namespace MCMBridge;
	const auto        measure = [](std::string_view a_text) { return static_cast<float>(a_text.size()); };
	const std::string source = "Hotkey selection";
	CHECK(FitControlText(source, 30, measure) == source);
	CHECK(FitControlText(source, 8, measure) == "Hotke...");
	CHECK(source == "Hotkey selection");
	CHECK(FitControlText(source, 2, measure).empty());
	CHECK(FitControlText(source, 0, measure).empty());
	const std::string translated = "\xC3\x84\xC3\x96\xC3\x9C";
	CHECK(FitControlText(translated, 5, measure) == "\xC3\x84...");
	CHECK(FitControlText(translated, 4, measure) == "...");
}

TEST_CASE("MCM content scales gently with available width and multiline labels stack")
{
	using namespace MCMBridge;
	CHECK(FitMCMContentScale(1200, 30) == 1);
	CHECK(FitMCMContentScale(600, 30) == 0.8F);
	CHECK(FitMCMContentScale(2400, 30) == 1.1F);
	CHECK(FitMCMContentScale(100, 0) == 1);
	CHECK(FitControlRow(600, 50, 240, 50, 8, true).stacked);
}

TEST_CASE("Rich text wraps whole words across color boundaries and retains empty lines")
{
	using namespace MCMBridge;
	const auto               measure = [](std::string_view a_text) { return static_cast<float>(a_text.size()); };
	const auto               text = ParseSkyUIRichText("Disable <font color='#ff0000'>SMP hair</font> when there is a wig");
	const auto               lines = WrapRichText(text, 16, measure);
	std::vector<std::string> plain;
	for (const auto& line : lines) {
		std::string combined;
		for (const auto& span : line)
			combined += span.text;
		CHECK(combined.size() <= 16);
		plain.push_back(std::move(combined));
	}
	CHECK(plain == std::vector<std::string>{ "Disable SMP hair", "when there is a", "wig" });
	REQUIRE(lines.front().size() == 2);
	CHECK(lines.front()[1].color == 0xff0000U);
	const auto explicitLines = WrapRichText(ParseSkyUIRichText("First\n\nLast\n"), 100, measure);
	REQUIRE(explicitLines.size() == 4);
	CHECK(explicitLines[1].empty());
	CHECK(explicitLines.back().empty());
}

TEST_CASE("Overlong rich text words wrap only at UTF-8 boundaries")
{
	using namespace MCMBridge;
	const auto measure = [](std::string_view a_text) { return static_cast<float>(a_text.size()); };
	const auto text = ParseSkyUIRichText("\xC3\x84\xC3\x96\xC3\x9C");
	const auto lines = WrapRichText(text, 3, measure);
	REQUIRE(lines.size() == 3);
	std::string combined;
	for (const auto& line : lines) {
		REQUIRE(line.size() == 1);
		CHECK(line.front().text.size() == 2);
		combined += line.front().text;
	}
	CHECK(combined == text.plainText);
	CHECK(WrapRichText(text, 0, measure).size() == 3);
}
