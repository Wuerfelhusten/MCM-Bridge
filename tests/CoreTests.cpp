#include "MCMBridge/Core/ClassicParser.h"
#include "MCMBridge/Core/Color.h"
#include "MCMBridge/Core/FrameworkMenuPath.h"
#include "MCMBridge/Core/HostAudit.h"
#include "MCMBridge/Core/HostedPageMerge.h"
#include "MCMBridge/Core/MenuOptionResolver.h"
#include "MCMBridge/Core/OperationContext.h"
#include "MCMBridge/Core/RegistrySettler.h"
#include "MCMBridge/Core/SkyUIFormat.h"
#include "MCMBridge/Core/SkyUIRichText.h"
#include "MCMBridge/Core/Slider.h"
#include "MCMBridge/Core/StableId.h"
#include "MCMBridge/Snapshot/SnapshotStore.h"
#include "MCMBridge/Write/WriteValidator.h"

#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>

TEST_CASE("Slider comparison ignores only bounded representation noise")
{
	using namespace MCMBridge;
	SliderMetadata metadata{ .minimum = 0.0F, .maximum = 10.0F, .step = 0.05F };
	CHECK(SliderValuesEqual(2.1500000953674316F, 2.1499998569488525F, metadata));
	CHECK_FALSE(SliderValuesEqual(2.15F, 2.20F, metadata));
	CHECK_FALSE(SliderValuesEqual(2.15F, 2.151F, metadata));
	CHECK_FALSE(SliderValuesEqual(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), metadata));
	CHECK_FALSE(SliderValuesEqual(std::numeric_limits<float>::quiet_NaN(), 2.15F, metadata));
	metadata.step = 0;
	CHECK_FALSE(SliderValuesEqual(2.15F, 2.15F, metadata));
	metadata.step = 0.0000001F;
	CHECK_FALSE(SliderValuesEqual(2.15F, std::nextafter(2.15F, 3.0F), metadata));
	REQUIRE(NormalizeSliderValue(2.15F, 0.0F, 10.0F, 0.05F));
	CHECK(*NormalizeSliderValue(2.15F, 0.0F, 10.0F, 0.05F) == 2.15F);
}

TEST_CASE("Slider normalization preserves aligned zero without hiding real off-grid values")
{
	using namespace MCMBridge;
	for (const float minimum : { -0.75F, -1.0F, -200.0F }) {
		REQUIRE(NormalizeSliderValue(0.0F, minimum, 200.0F, 0.05F));
		CHECK(*NormalizeSliderValue(0.0F, minimum, 200.0F, 0.05F) == 0.0F);
	}
	CHECK(*NormalizeSliderValue(0.0F, -0.73F, 1.0F, 0.05F) != 0.0F);
	CHECK(*NormalizeSliderValue(0.01F, -1.0F, 1.0F, 0.05F) != 0.01F);
	CHECK(*NormalizeSliderValue(0.0F, 0.1F, 1.0F, 0.05F) == 0.1F);
	const float tiny = 0.00000001F;
	CHECK(*NormalizeSliderValue(tiny, 0.0F, 1.0F, tiny) == tiny);
	CHECK(*NormalizeSliderValue(0.025F, 0.0F, 1.0F, 0.05F) == 0.05F);
	CHECK(*NormalizeSliderValue(2.15F, 0.0F, 10.0F, 0.05F) == 2.15F);
}

TEST_CASE("Restore audit preserves value types and escapes profile text")
{
	using namespace MCMBridge;
	CHECK(AuditValue(std::monostate{}) == "none");
	CHECK(AuditValue(true) == "bool:true");
	CHECK(AuditValue(std::int32_t(-1)) == "int:-1");
	CHECK(AuditValue(std::uint32_t(255)) == "uint:255");
	CHECK(AuditValue(0.5F) == "float:0.5");
	CHECK(AuditValue(std::string("a\n\"b")) == "string:\"a\\n\\\"b\"");
	CHECK(AuditIntent(WriteIntent::kActivate) == "ACTIVATE");
	CHECK(AuditIntent(WriteIntent::kReset) == "RESET");
	CHECK(AuditIntent(WriteIntent::kSetValue) == "SET_VALUE");
}

namespace
{
	class FakeClock final : public MCMBridge::IOperationClock
	{
	public:
		TimePoint Now() const override
		{
			return now;
		}

		void Advance(std::chrono::steady_clock::duration a_duration)
		{
			now += a_duration;
		}

	private:
		TimePoint now{};
	};
}

TEST_CASE("Stable IDs are deterministic and scoped")
{
	const auto first = MCMBridge::MakeClassicModID("Fixture.esp", 0x1234, "FixtureConfig");
	const auto second = MCMBridge::MakeClassicModID("Fixture.esp", 0x1234, "FixtureConfig");
	const auto different = MCMBridge::MakeClassicModID("Fixture.esp", 0x1235, "FixtureConfig");
	CHECK(first == second);
	CHECK(first != different);

	const auto helper = MCMBridge::MakeHelperControlID("Fixture.esp", 0x1234, "FixtureConfig", "General", "menu", "iMode:General");
	const auto otherHelper = MCMBridge::MakeHelperControlID("Other.esp", 0x1234, "FixtureConfig", "General", "menu", "iMode:General");
	CHECK(helper != otherHelper);
}

TEST_CASE("Classic buffers decode supported controls and flags")
{
	MCMBridge::ClassicPageContext context{
		.modID = "classic-mod:test",
		.ownerPlugin = "Fixture.esp",
		.questFormID = 0x1234,
		.scriptName = "FixtureConfig",
		.pageName = "General",
		.pageIndex = 0
	};
	MCMBridge::ClassicPageBuffers buffers{
		.optionFlags = { 1, 3, 4, 5 + 256 },
		.labels = { "Header", "Toggle", "Slider", "Menu" },
		.stringValues = { "", "", "{0}", "One" },
		.numericValues = { 0.0F, 1.0F, 2.5F, 0.0F },
		.stateNames = { "", "toggleState", "", "" }
	};

	auto page = MCMBridge::ParseClassicPage(context, buffers);
	REQUIRE(page);
	REQUIRE(page->controls.size() == 4);
	CHECK(page->controls[1].type == MCMBridge::MCMControlType::kToggle);
	CHECK(std::get<bool>(page->controls[1].value));
	CHECK(page->controls[1].identity.confidence == MCMBridge::IdentityConfidence::kHigh);
	CHECK(page->controls[2].writeCapability == MCMBridge::WriteCapability::kWritable);
	CHECK(page->controls[3].writeCapability == MCMBridge::WriteCapability::kDisabled);
}

TEST_CASE("Classic buffers retain keymap controls")
{
	MCMBridge::ClassicPageContext context{
		.modID = "classic-mod:test",
		.ownerPlugin = "Fixture.esp",
		.questFormID = 0x1234,
		.scriptName = "FixtureConfig",
		.pageName = "Controls",
		.pageIndex = 1
	};
	MCMBridge::ClassicPageBuffers buffers{
		.optionFlags = { 7 },
		.labels = { "Hotkey" },
		.stringValues = { "" },
		.numericValues = { 42.0F },
		.stateNames = { "hotkeyState" }
	};

	auto page = MCMBridge::ParseClassicPage(context, buffers);
	REQUIRE(page);
	REQUIRE(page->controls.size() == 1);
	CHECK(page->controls.front().type == MCMBridge::MCMControlType::kKeymap);
	CHECK(std::get<std::int32_t>(page->controls.front().value) == 42);
	CHECK(page->controls.front().writeCapability == MCMBridge::WriteCapability::kWritable);
}

TEST_CASE("Classic keymaps preserve SkyUI unmap capability")
{
	MCMBridge::ClassicPageContext context{
		.modID = "classic-mod:test",
		.ownerPlugin = "Fixture.esp",
		.questFormID = 0x1234,
		.scriptName = "FixtureConfig",
		.pageName = "Controls",
		.pageIndex = 1
	};
	MCMBridge::ClassicPageBuffers buffers{
		.optionFlags = { 7 + (4 << 8) },
		.labels = { "Hotkey" },
		.stringValues = { "" },
		.numericValues = { 42.0F },
		.stateNames = { "hotkeyState" }
	};

	auto page = MCMBridge::ParseClassicPage(context, buffers);
	REQUIRE(page);
	REQUIRE(page->controls.size() == 1);
	CHECK(page->controls.front().allowUnmap);
}

TEST_CASE("Classic text controls expose callback activation")
{
	MCMBridge::ClassicPageContext context{
		.modID = "classic-mod:test",
		.ownerPlugin = "Fixture.esp",
		.questFormID = 0x1234,
		.scriptName = "FixtureConfig",
		.pageName = "General",
		.pageIndex = 0
	};
	MCMBridge::ClassicPageBuffers buffers{
		.optionFlags = { 2 },
		.labels = { "Size" },
		.stringValues = { "Medium" },
		.numericValues = { 0.0F },
		.stateNames = { "sizeState" }
	};

	auto page = MCMBridge::ParseClassicPage(context, buffers);
	REQUIRE(page);
	REQUIRE(page->controls.size() == 1);
	CHECK(page->controls.front().type == MCMBridge::MCMControlType::kText);
	CHECK(std::get<std::string>(page->controls.front().value) == "Medium");
	CHECK(page->controls.front().writeCapability == MCMBridge::WriteCapability::kWritable);
}

TEST_CASE("Classic buffers preserve sparse two-column positions")
{
	MCMBridge::ClassicPageContext context{
		.modID = "classic-mod:test",
		.ownerPlugin = "Fixture.esp",
		.questFormID = 0x1234,
		.scriptName = "FixtureConfig",
		.pageName = "General",
		.pageIndex = 0
	};
	MCMBridge::ClassicPageBuffers buffers{
		.optionFlags = { 1, 0, 3 },
		.labels = { "Left header", "", "Left toggle" },
		.stringValues = { "", "", "" },
		.numericValues = { 0.0F, 0.0F, 1.0F },
		.stateNames = { "", "", "toggleState" }
	};

	auto page = MCMBridge::ParseClassicPage(context, buffers);
	REQUIRE(page);
	REQUIRE(page->controls.size() == 3);
	CHECK(page->controls[1].type == MCMBridge::MCMControlType::kEmpty);
	CHECK(page->controls[1].layout.position == 1);
	CHECK(page->controls[1].layout.column == 1);
	CHECK(page->controls[2].layout.position == 2);
	CHECK(page->controls[2].layout.column == 0);
}

TEST_CASE("Slider values are clamped and aligned to the step")
{
	auto value = MCMBridge::NormalizeSliderValue(9.7F, 0.0F, 10.0F, 0.5F);
	REQUIRE(value);
	CHECK(*value == 9.5F);

	value = MCMBridge::NormalizeSliderValue(20.0F, 0.0F, 10.0F, 1.0F);
	REQUIRE(value);
	CHECK(*value == 10.0F);

	CHECK_FALSE(MCMBridge::NormalizeSliderValue(1.0F, 5.0F, 1.0F, 1.0F));
}

TEST_CASE("SkyUI slider formats preserve precision and suffixes")
{
	CHECK(MCMBridge::MakeSliderPrintfFormat("{0}") == "%.0f");
	CHECK(MCMBridge::MakeSliderPrintfFormat("{1}") == "%.1f");
	CHECK(MCMBridge::MakeSliderPrintfFormat("{0} s") == "%.0f s");
	CHECK(MCMBridge::MakeSliderPrintfFormat("{2}%") == "%.2f%%");
	CHECK(MCMBridge::MakeSliderPrintfFormat("invalid") == "%.2f");
}

TEST_CASE("SkyUI rich text exposes font colors without leaking markup")
{
	const auto parsed = MCMBridge::ParseSkyUIRichText(
		"Status: <font color='#dd3333'>Not &amp; taken</font>");
	CHECK(parsed.plainText == "Status: Not & taken");
	REQUIRE(parsed.spans.size() == 2);
	CHECK_FALSE(parsed.spans[0].color);
	REQUIRE(parsed.spans[1].color);
	CHECK(*parsed.spans[1].color == 0xDD3333U);
	CHECK(parsed.spans[1].text == "Not & taken");
}

TEST_CASE("SkyUI rich text strips unsupported tags and preserves comparisons")
{
	CHECK(MCMBridge::PlainSkyUIText("<b>Bold</b><br>Next") == "Bold\nNext");
	CHECK(MCMBridge::PlainSkyUIText("Value < 5 > 2") == "Value < 5 > 2");
	CHECK(MCMBridge::PlainSkyUIText("<font color='#bad'>Fallback</font>") == "Fallback");
}

TEST_CASE("ARGB colors round trip through the renderer representation")
{
	const auto source = static_cast<std::uint32_t>(0x7F123456U);
	const auto components = MCMBridge::UnpackARGB(source);
	CHECK(MCMBridge::PackARGB(components) == source);
}

TEST_CASE("Writes validate identity and value instead of unrelated generation changes")
{
	MCMBridge::SnapshotStore store;
	MCMBridge::MCMControl    control;
	control.identity.stableID = "setting:test";
	control.value = true;
	control.writeCapability = MCMBridge::WriteCapability::kWritable;

	MCMBridge::MCMPage page;
	page.controls.push_back(control);
	MCMBridge::MCMMod mod;
	mod.pages.push_back(page);
	MCMBridge::MCMSnapshot next;
	next.mods.push_back(mod);
	const auto snapshot = store.Publish(std::move(next));

	MCMBridge::WriteCommand valid{
		.snapshotGeneration = snapshot->generation,
		.settingID = "setting:test",
		.expectedIdentity = control.identity,
		.expectedType = control.type,
		.expectedValue = true,
		.desiredValue = false
	};
	CHECK(MCMBridge::ValidateWrite(*snapshot, valid));

	auto older = valid;
	older.snapshotGeneration = snapshot->generation - 1;
	CHECK(MCMBridge::ValidateWrite(*snapshot, older));

	auto future = valid;
	future.snapshotGeneration = snapshot->generation + 1;
	auto result = MCMBridge::ValidateWrite(*snapshot, future);
	REQUIRE_FALSE(result);
	CHECK(result.error().code == MCMBridge::BridgeErrorCode::kStaleSnapshot);

	auto changedType = valid;
	changedType.expectedType = MCMBridge::MCMControlType::kSlider;
	result = MCMBridge::ValidateWrite(*snapshot, changedType);
	REQUIRE_FALSE(result);
	CHECK(result.error().code == MCMBridge::BridgeErrorCode::kStaleSnapshot);

	auto changedIdentity = valid;
	changedIdentity.expectedIdentity.pageKey = "Other";
	result = MCMBridge::ValidateWrite(*snapshot, changedIdentity);
	REQUIRE_FALSE(result);
	CHECK(result.error().code == MCMBridge::BridgeErrorCode::kStaleSnapshot);

	auto invalidActivation = valid;
	invalidActivation.intent = MCMBridge::WriteIntent::kActivate;
	result = MCMBridge::ValidateWrite(*snapshot, invalidActivation);
	REQUIRE_FALSE(result);
	CHECK(result.error().code == MCMBridge::BridgeErrorCode::kUnsupported);

	auto reset = valid;
	reset.intent = MCMBridge::WriteIntent::kReset;
	CHECK(MCMBridge::ValidateWrite(*snapshot, reset));
}

TEST_CASE("Committed writes use freshly read values and reject a value reverted by close")
{
	using namespace MCMBridge;
	const std::vector<std::pair<MCMControlType, MCMValue>> cases{
		{ MCMControlType::kToggle, true }, { MCMControlType::kSlider, 0.5F },
		{ MCMControlType::kMenu, std::int32_t{ 2 } }, { MCMControlType::kColor, std::uint32_t{ 0xFF112233 } },
		{ MCMControlType::kKeymap, std::int32_t{ 42 } }, { MCMControlType::kInput, std::string("new") }
	};
	for (const auto& [type, value] : cases) {
		MCMSnapshot snapshot;
		snapshot.mods.resize(1);
		snapshot.mods[0].pages.resize(1);
		auto& controls = snapshot.mods[0].pages[0].controls;
		controls.resize(1);
		auto& control = controls[0];
		control.identity.stableID = "setting";
		control.type = type;
		control.value = value;
		control.disabled = true;
		WriteCommand command{ .settingID = "setting", .expectedIdentity = control.identity, .expectedType = type };
		const auto   result = ConfirmWrite(snapshot, command, value);
		REQUIRE(result);
		CHECK(*result == value);
		std::visit([](auto& a_value) {
			using T = std::decay_t<decltype(a_value)>;
			if constexpr (std::is_same_v<T, bool>)
				a_value = !a_value;
			else if constexpr (std::is_arithmetic_v<T>)
				++a_value;
			else if constexpr (std::is_same_v<T, std::string>)
				a_value += " old";
		},
			control.value);
		CHECK_FALSE(ConfirmWrite(snapshot, command, value));
		control.value = value;
		CHECK_FALSE(ConfirmWrite(snapshot, command, MCMValue{}));
		control.identity.stateName = "changed";
		CHECK_FALSE(ConfirmWrite(snapshot, command, value));
		control.identity = command.expectedIdentity;
		control.type = MCMControlType::kUnknown;
		CHECK_FALSE(ConfirmWrite(snapshot, command, value));
		control.type = type;
		controls.push_back(control);
		CHECK_FALSE(ConfirmWrite(snapshot, command, value));
		controls.clear();
		CHECK_FALSE(ConfirmWrite(snapshot, command, value));
	}
}

TEST_CASE("Reset and activation report their post-commit value rather than the pre-close value")
{
	using namespace MCMBridge;
	MCMSnapshot snapshot;
	snapshot.mods.resize(1);
	snapshot.mods[0].pages.resize(1);
	snapshot.mods[0].pages[0].controls.resize(1);
	auto& control = snapshot.mods[0].pages[0].controls[0];
	control.identity.stableID = "setting";
	control.type = MCMControlType::kText;
	control.value = std::string("after close");
	WriteCommand command{ .settingID = "setting", .expectedIdentity = control.identity, .expectedType = MCMControlType::kStepper, .intent = WriteIntent::kActivate };
	const auto   activated = ConfirmWrite(snapshot, command, std::string("before close"));
	REQUIRE(activated);
	CHECK(std::get<std::string>(*activated) == "after close");
	command.intent = WriteIntent::kReset;
	command.expectedType = MCMControlType::kToggle;
	control.type = MCMControlType::kToggle;
	control.value = false;
	const auto reset = ConfirmWrite(snapshot, command, true);
	REQUIRE(reset);
	CHECK(std::get<bool>(*reset) == false);
	control.value = std::monostate{};
	CHECK_FALSE(ConfirmWrite(snapshot, command, true));
}

TEST_CASE("Reset remains available when menu choices are unavailable")
{
	MCMBridge::MCMControl control;
	control.type = MCMBridge::MCMControlType::kMenu;
	control.identity.stableID = "setting:menu";
	control.value = static_cast<std::int32_t>(1);
	control.writeCapability = MCMBridge::WriteCapability::kMissingOptions;
	MCMBridge::MCMPage page;
	page.controls.push_back(control);
	MCMBridge::MCMMod mod;
	mod.pages.push_back(page);
	MCMBridge::MCMSnapshot snapshot;
	snapshot.generation = 3;
	snapshot.mods.push_back(mod);
	MCMBridge::WriteCommand command{
		.snapshotGeneration = 3,
		.settingID = control.identity.stableID,
		.expectedIdentity = control.identity,
		.expectedType = control.type,
		.expectedValue = control.value,
		.desiredValue = std::monostate{},
		.intent = MCMBridge::WriteIntent::kReset
	};

	CHECK(MCMBridge::ValidateWrite(snapshot, command));
}

TEST_CASE("Transient write states do not invalidate queued commands")
{
	MCMBridge::SnapshotStore store;
	MCMBridge::MCMControl    control;
	control.identity.stableID = "setting:test";
	control.value = true;
	control.writeCapability = MCMBridge::WriteCapability::kWritable;
	MCMBridge::MCMPage page;
	page.controls.push_back(control);
	MCMBridge::MCMMod mod;
	mod.pages.push_back(page);
	MCMBridge::MCMSnapshot source;
	source.mods.push_back(mod);
	const auto initial = store.Publish(std::move(source));

	const auto pending = store.SetWriteStatus("setting:test", MCMBridge::WriteStatus::kPending);
	CHECK(pending->generation == initial->generation);
	const auto applied = store.SetWriteStatus(
		"setting:test", MCMBridge::WriteStatus::kApplied, MCMBridge::MCMValue(false));
	CHECK(applied->generation == initial->generation + 1);
}

TEST_CASE("Hosted page refresh preserves metadata and uses the live menu value")
{
	MCMBridge::MCMControl previousControl;
	previousControl.identity.stableID = "setting:menu";
	previousControl.identity.optionIndex = 3;
	previousControl.identity.stateName = "MenuState";
	previousControl.type = MCMBridge::MCMControlType::kMenu;
	previousControl.value = static_cast<std::int32_t>(0);
	previousControl.defaultValue = static_cast<std::int32_t>(1);
	previousControl.writeCapability = MCMBridge::WriteCapability::kWritable;
	previousControl.menu = MCMBridge::MenuMetadata{
		.options = { "$Small", "$Medium", "$Large" },
		.selectedIndex = 0,
		.defaultIndex = 1,
		.availability = MCMBridge::MetadataAvailability::kAvailable
	};
	MCMBridge::MCMPage previous;
	previous.controls.push_back(previousControl);

	MCMBridge::MCMControl currentControl;
	currentControl.identity.stableID = "positional:menu";
	currentControl.identity.optionIndex = 3;
	currentControl.identity.stateName = "MenuState";
	currentControl.type = MCMBridge::MCMControlType::kMenu;
	currentControl.displayValue = "$Large";
	currentControl.menu = MCMBridge::MenuMetadata{};
	MCMBridge::MCMPage current;
	current.controls.push_back(currentControl);

	MCMBridge::MergeHostedPageState(previous, current);
	const auto& merged = current.controls.front();
	CHECK(merged.identity.stableID == "setting:menu");
	CHECK(merged.writeCapability == MCMBridge::WriteCapability::kWritable);
	REQUIRE(merged.menu);
	CHECK(merged.menu->options.size() == 3);
	CHECK(merged.menu->selectedIndex == 2);
	CHECK(std::get<std::int32_t>(merged.value) == 2);
}

TEST_CASE("Text writes preserve raw values separately from localized display text")
{
	MCMBridge::SnapshotStore store;
	MCMBridge::MCMControl    control;
	control.type = MCMBridge::MCMControlType::kText;
	control.identity.stableID = "setting:text";
	control.value = std::string("$Medium");
	control.displayValue = "Mittel";
	control.writeCapability = MCMBridge::WriteCapability::kWritable;

	MCMBridge::MCMPage page;
	page.controls.push_back(control);
	MCMBridge::MCMMod mod;
	mod.pages.push_back(page);
	MCMBridge::MCMSnapshot next;
	next.mods.push_back(mod);
	const auto snapshot = store.Publish(std::move(next));

	MCMBridge::WriteCommand command{
		.snapshotGeneration = snapshot->generation,
		.settingID = control.identity.stableID,
		.expectedIdentity = control.identity,
		.expectedType = control.type,
		.expectedValue = std::string("$Medium"),
		.desiredValue = std::monostate{},
		.intent = MCMBridge::WriteIntent::kActivate
	};
	CHECK(MCMBridge::ValidateWrite(*snapshot, command));

	const auto applied = store.SetWriteStatus(
		control.identity.stableID,
		MCMBridge::WriteStatus::kApplied,
		std::string("$Large"),
		std::string("Gross"));
	const auto& updated = applied->mods.front().pages.front().controls.front();
	CHECK(std::get<std::string>(updated.value) == "$Large");
	CHECK(updated.displayValue == "Gross");
}

TEST_CASE("Menu option capture accepts only the scoped target")
{
	MCMBridge::ScopedMenuOptionResolver resolver;
	MCMBridge::SettingIdentity          identity;
	identity.stableID = "menu:test";
	resolver.BeginCapture(identity);
	resolver.ObserveInvokeStringArray("Journal Menu", "otherTarget", { "Ignored" });
	CHECK_FALSE(resolver.Resolve(identity));

	resolver.BeginCapture(identity);
	resolver.ObserveInvokeStringArray("Other Menu", "_root.ConfigPanelFader.configPanel.setMenuDialogOptions", { "Ignored" });
	CHECK_FALSE(resolver.Resolve(identity));

	resolver.BeginCapture(identity);
	resolver.ObserveInvokeStringArray("Journal Menu", "_root.ConfigPanelFader.configPanel.setMenuDialogOptions", { "One", "Two" });
	auto result = resolver.Resolve(identity);
	REQUIRE(result);
	CHECK(result->options.size() == 2);
}

TEST_CASE("Registry settling requires two unchanged observations")
{
	MCMBridge::RegistrySettler settler;
	CHECK(settler.Observe({ "b", "a" }) == MCMBridge::RegistrySettleResult::kChanged);
	CHECK(settler.Observe({ "a", "b" }) == MCMBridge::RegistrySettleResult::kWaiting);
	CHECK(settler.Observe({ "b", "a" }) == MCMBridge::RegistrySettleResult::kReady);
	CHECK_FALSE(settler.ShouldContinue());

	settler.Reset();
	CHECK(settler.Observe({}) == MCMBridge::RegistrySettleResult::kEmpty);
	CHECK(settler.ShouldContinue());
}

TEST_CASE("Registry settling detects later SkyUI registrations after polling stops")
{
	MCMBridge::RegistrySettler settler;
	CHECK(settler.Observe({ "first" }) == MCMBridge::RegistrySettleResult::kChanged);
	CHECK(settler.Observe({ "first" }) == MCMBridge::RegistrySettleResult::kWaiting);
	CHECK(settler.Observe({ "first" }) == MCMBridge::RegistrySettleResult::kReady);
	CHECK_FALSE(settler.ShouldContinue());
	CHECK(settler.Observe({ "first", "second" }) == MCMBridge::RegistrySettleResult::kChanged);
}

TEST_CASE("Operation context rejects late callbacks, timeouts, and invalid sessions")
{
	FakeClock                   clock;
	MCMBridge::OperationContext operation(std::chrono::seconds(10), clock);
	operation.Start();
	const auto first = operation.BeginStep();
	CHECK(operation.IsCurrent(first));

	const auto second = operation.BeginStep();
	CHECK_FALSE(operation.IsCurrent(first));
	CHECK(operation.IsCurrent(second));

	clock.Advance(std::chrono::seconds(10));
	CHECK_FALSE(operation.IsExpired());
	clock.Advance(std::chrono::milliseconds(1));
	CHECK(operation.IsExpired());

	operation.Start();
	const auto nextSession = operation.BeginStep();
	CHECK(operation.IsCurrent(nextSession));
	operation.Invalidate();
	CHECK_FALSE(operation.IsCurrent(nextSession));
}

TEST_CASE("Framework menu paths preserve literal slash characters")
{
	CHECK(MCMBridge::EscapeFrameworkMenuPathSegment("General") == "General");
	CHECK(MCMBridge::EscapeFrameworkMenuPathSegment("Hall / Gallery") == "Hall \\/ Gallery");
	CHECK(MCMBridge::EscapeFrameworkMenuPathSegment("A\\/B") == "A\\\\/B");
	CHECK(MCMBridge::JoinFrameworkMenuPath("Legacy / Museum", "Hall / 1") ==
		  "Legacy \\/ Museum/Hall \\/ 1");
}

TEST_CASE("MCM grouping adds only an owned display folder")
{
	const std::string section = "Alias / Museum##MCMBridge-original-id";
	const auto        root = MCMBridge::MCMFrameworkSectionPath(section, false);
	const auto        grouped = MCMBridge::MCMFrameworkSectionPath(section, true);
	CHECK(root == MCMBridge::EscapeFrameworkMenuPathSegment(section));
	CHECK(grouped == std::string(MCMBridge::mcmFolderPath) + "/" + root);
	CHECK(MCMBridge::JoinFrameworkMenuPath(section, "Hall / 1", true) ==
		  std::string(MCMBridge::mcmFolderPath) + "/" + MCMBridge::JoinFrameworkMenuPath(section, "Hall / 1"));
	CHECK(MCMBridge::MCMFrameworkSectionPath(section, false) == root);
}
