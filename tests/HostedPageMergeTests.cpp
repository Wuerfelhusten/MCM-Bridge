#include "MCMBridge/Core/ClassicParser.h"
#include "MCMBridge/Core/HostedPageMerge.h"
#include "MCMBridge/Snapshot/SnapshotStore.h"
#include "MCMBridge/Write/WriteValidator.h"

#include <catch2/catch_test_macros.hpp>

#include <array>

namespace
{
	using namespace MCMBridge;

	MCMPage ReadPage(std::int32_t a_type, bool a_disabled)
	{
		const ClassicPageContext context{
			.modID = "fixture",
			.ownerPlugin = "Fixture.esp",
			.questFormID = 0x1234,
			.scriptName = "FixtureConfig",
			.pageName = "Dependencies"
		};
		const ClassicPageBuffers buffers{
			.optionFlags = { a_type | (a_disabled ? 0x100 : 0) },
			.labels = { "Dependent option" },
			.stringValues = { "First" },
			.numericValues = { 0.0F },
			.stateNames = { "DependentState" }
		};
		auto page = ParseClassicPage(context, buffers);
		REQUIRE(page);
		REQUIRE(page->controls.size() == 1);
		return std::move(*page);
	}

	void AddMetadata(MCMControl& a_control, MCMBackendKind a_backend, bool a_stepper = false)
	{
		a_control.identity.backend = a_backend;
		if (a_backend == MCMBackendKind::kMCMHelper) {
			a_control.identity.stableID = "helper:dependent";
			a_control.identity.explicitID = "dependent:General";
		}
		if (a_stepper)
			a_control.type = MCMControlType::kStepper;
		if (a_control.type == MCMControlType::kText && a_backend == MCMBackendKind::kMCMHelper) {
			a_control.action = ActionMetadata{ .type = "CallFunction", .functionName = "OnAction" };
		}
		if (a_control.type == MCMControlType::kSlider) {
			a_control.slider = SliderMetadata{ .maximum = 10.0F, .availability = MetadataAvailability::kAvailable };
		}
		if (a_control.type == MCMControlType::kMenu || a_control.type == MCMControlType::kStepper) {
			a_control.menu = MenuMetadata{
				.options = { "First", "Second" },
				.selectedIndex = 0,
				.availability = MetadataAvailability::kAvailable
			};
			a_control.value = std::int32_t{ 0 };
		}
		if (a_control.type == MCMControlType::kColor) {
			a_control.color = ColorMetadata{ .availability = MetadataAvailability::kAvailable };
		}
	}

	WriteCommand MakeCommand(const MCMSnapshot& a_snapshot, WriteIntent a_intent)
	{
		const auto& control = a_snapshot.mods.front().pages.front().controls.front();
		return {
			.snapshotGeneration = a_snapshot.generation,
			.settingID = control.identity.stableID,
			.expectedIdentity = control.identity,
			.expectedType = control.type,
			.expectedValue = control.value,
			.desiredValue = true,
			.intent = a_intent
		};
	}
}

TEST_CASE("Hosted refresh re-enables dependent controls across repeated flag changes")
{
	for (const auto backend : { MCMBackendKind::kClassicSkyUI, MCMBackendKind::kMCMHelper }) {
		for (const auto type : { 2, 3, 4, 5, 6, 7, 8 }) {
			CAPTURE(backend, type);
			auto previous = ReadPage(type, true);
			AddMetadata(previous.controls.front(), backend);
			const auto identity = previous.controls.front().identity;
			for (const auto disabled : { false, true, false, false }) {
				CAPTURE(disabled);
				auto current = ReadPage(type, disabled);
				MergeHostedPageState(previous, current);
				const auto& control = current.controls.front();
				CHECK(control.disabled == disabled);
				CHECK(control.writeCapability == (disabled ? WriteCapability::kDisabled : WriteCapability::kWritable));
				CHECK(control.identity == identity);
				previous = std::move(current);
			}
		}
	}
}

TEST_CASE("Hosted refresh restores helper steppers after merging their menu metadata")
{
	auto previous = ReadPage(2, true);
	AddMetadata(previous.controls.front(), MCMBackendKind::kMCMHelper, true);
	for (const auto disabled : { false, true, false }) {
		auto current = ReadPage(2, disabled);
		MergeHostedPageState(previous, current);
		const auto& control = current.controls.front();
		CHECK(control.type == MCMControlType::kStepper);
		CHECK(control.writeCapability == (disabled ? WriteCapability::kDisabled : WriteCapability::kWritable));
		REQUIRE(control.menu);
		CHECK(control.menu->options.size() == 2);
		CHECK(std::get<std::int32_t>(control.value) == 0);
		previous = std::move(current);
	}
}

TEST_CASE("Hosted refresh publishes enabled toggles for both normal writes and resets")
{
	SnapshotStore store;
	auto          previous = ReadPage(3, true);
	const auto    initial = store.Publish(MCMSnapshot{
		.mods = { MCMMod{ .stableID = "fixture", .pages = { previous } } } });
	CHECK_FALSE(ValidateWrite(*initial, MakeCommand(*initial, WriteIntent::kSetValue)));
	CHECK_FALSE(ValidateWrite(*initial, MakeCommand(*initial, WriteIntent::kReset)));

	for (const auto disabled : { false, true, false }) {
		auto current = ReadPage(3, disabled);
		MergeHostedPageState(previous, current);
		const auto snapshot = store.ReplacePage("fixture", current);
		CHECK(ValidateWrite(*snapshot, MakeCommand(*snapshot, WriteIntent::kSetValue)).has_value() == !disabled);
		CHECK(ValidateWrite(*snapshot, MakeCommand(*snapshot, WriteIntent::kReset)).has_value() == !disabled);
		CHECK(snapshot->generation > initial->generation);
		CHECK(initial->mods.front().pages.front().controls.front().disabled);
		CHECK(initial->mods.front().pages.front().controls.front().writeCapability == WriteCapability::kDisabled);
		previous = std::move(current);
	}
}

TEST_CASE("Hosted refresh retains metadata restrictions when disabled controls become enabled")
{
	const std::array cases{
		std::pair{ 4, WriteCapability::kReadOnly },
		std::pair{ 5, WriteCapability::kMissingOptions },
		std::pair{ 6, WriteCapability::kReadOnly }
	};
	for (const auto& [type, expected] : cases) {
		CAPTURE(type);
		auto previous = ReadPage(type, true);
		for (const auto disabled : { false, true, false }) {
			auto current = ReadPage(type, disabled);
			MergeHostedPageState(previous, current);
			CHECK(current.controls.front().writeCapability == (disabled ? WriteCapability::kDisabled : expected));
			previous = std::move(current);
		}
	}
}

TEST_CASE("Hosted refresh keeps helper text without an action read-only after enabling")
{
	auto previous = ReadPage(2, true);
	previous.controls.front().identity.backend = MCMBackendKind::kMCMHelper;
	for (const auto disabled : { false, true, false }) {
		auto current = ReadPage(2, disabled);
		MergeHostedPageState(previous, current);
		CHECK(current.controls.front().writeCapability == (disabled ? WriteCapability::kDisabled : WriteCapability::kReadOnly));
		previous = std::move(current);
	}
}

TEST_CASE("Hosted refresh does not discard explicit write restrictions or pending status")
{
	for (const auto capability : { WriteCapability::kReadOnly, WriteCapability::kUnsupported }) {
		auto previous = ReadPage(3, false);
		previous.controls.front().writeCapability = capability;
		previous.controls.front().writeStatus = WriteStatus::kPending;
		auto current = ReadPage(3, false);
		MergeHostedPageState(previous, current);
		CHECK(current.controls.front().writeCapability == capability);
		CHECK(current.controls.front().writeStatus == WriteStatus::kPending);
	}
}

TEST_CASE("Identical live page refreshes preserve snapshots and queued command generations")
{
	SnapshotStore store;
	const auto    page = ReadPage(3, false);
	const auto    initial = store.Publish(MCMSnapshot{ .mods = { MCMMod{ .stableID = "fixture", .pages = { page } } } });
	const auto    command = MakeCommand(*initial, WriteIntent::kSetValue);
	for (int pass = 0; pass < 5; ++pass) {
		const auto refreshed = store.ReplacePage("fixture", page);
		CHECK(refreshed == initial);
		CHECK(refreshed->createdAt == initial->createdAt);
		CHECK(ValidateWrite(*refreshed, command));
	}
}

TEST_CASE("Page deduplication does not hide delayed control or metadata changes")
{
	SnapshotStore store;
	auto          page = ReadPage(4, false);
	AddMetadata(page.controls.front(), MCMBackendKind::kMCMHelper);
	const auto initial = store.Publish(MCMSnapshot{ .mods = { MCMMod{ .stableID = "fixture", .pages = { page } } } });
	auto       changed = page;
	auto&      control = changed.controls.front();
	SECTION("Value") { control.value = 2.0F; }
	SECTION("Disabled") { control.disabled = true; }
	SECTION("Hidden") { control.hidden = true; }
	SECTION("Capability") { control.writeCapability = WriteCapability::kDisabled; }
	SECTION("Write status") { control.writeStatus = WriteStatus::kApplied; }
	SECTION("Metadata validity") { control.metadataCurrent = false; }
	SECTION("Slider bounds") { control.slider->maximum = 50.0F; }
	SECTION("Menu options") { control.menu = MenuMetadata{ .options = { "New option" } }; }
	SECTION("Identity") { control.identity.stateName = "NewState"; }
	SECTION("Layout") { control.layout.column = 1; }
	SECTION("Title") { changed.title = "Changed title"; }
	SECTION("Navigation") { changed.rawName = "Changed page"; }
	SECTION("Custom content") { changed.customContent = CustomContentMetadata{ .source = "test.dds" }; }
	SECTION("Added control") { changed.controls.push_back(control); }
	SECTION("Removed control") { changed.controls.clear(); }
	const auto refreshed = store.ReplacePage("fixture", changed);
	CHECK(refreshed != initial);
	CHECK(refreshed->generation == initial->generation + 1);
	CHECK(initial->mods.front().pages.front() == page);
	CHECK(refreshed->mods.front().pages.front() == changed);
	CHECK(store.ReplacePage("fixture", changed) == refreshed);
}
