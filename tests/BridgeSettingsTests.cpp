#include "MCMBridge/Core/BridgeSettings.h"
#include "MCMBridge/Core/JournalRedirect.h"
#include "MCMBridge/Core/NativeJournalEntry.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <fstream>
#include <iterator>
#include <vector>

TEST_CASE("Native Journal admission preserves known category positions and rejects foreign catalogs")
{
	std::vector<std::string_view> labels{ "$QUICKSAVE", "$SAVE", "$LOAD", "$SETTINGS", "$CONTROLS", "$HELP", "$QUIT" };
	CHECK(MCMBridge::IsVanillaJournalCatalog(labels));
	labels.insert(labels.begin() + 3, "$INSTALLED CONTENT");
	CHECK(MCMBridge::IsVanillaJournalCatalog(labels));
	labels.insert(labels.begin() + 4, "$CREATIONS");
	CHECK(MCMBridge::IsVanillaJournalCatalog(labels));
	labels.push_back("Mod Configuration");
	CHECK_FALSE(MCMBridge::IsVanillaJournalCatalog(labels));
	labels.pop_back();
	std::swap(labels[0], labels[1]);
	CHECK_FALSE(MCMBridge::IsVanillaJournalCatalog(labels));
	labels.clear();
	CHECK_FALSE(MCMBridge::IsVanillaJournalCatalog(labels));
}

namespace
{
	struct SettingsFixture
	{
		SettingsFixture()
		{
			const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
			directory = std::filesystem::temp_directory_path() / ("MCMBridgeSettingsTest-" + std::to_string(suffix));
			std::filesystem::create_directory(directory);
		}
		~SettingsFixture()
		{
			std::error_code error;
			std::filesystem::remove_all(directory, error);
		}
		std::filesystem::path Path() const { return directory / "MCMBridge.ini"; }
		std::filesystem::path directory;
	};
}

TEST_CASE("Missing Bridge settings enable the pause without creating a file")
{
	SettingsFixture fixture;
	const auto      settings = MCMBridge::LoadBridgeSettings(fixture.Path());
	REQUIRE(settings);
	CHECK(settings->pauseDuringWrites);
	CHECK(settings->closeJournalOnRedirect);
	CHECK_FALSE(settings->groupMCMs);
	CHECK_FALSE(settings->alphabeticMCMs);
	CHECK(settings->mcmRangeEnds == "CGLRZ");
	CHECK_FALSE(std::filesystem::exists(fixture.Path()));
}

TEST_CASE("Aliases persist by stable ID and reset without changing general preferences")
{
	SettingsFixture           fixture;
	MCMBridge::BridgeSettings settings;
	MCMBridge::SetMCMAlias(settings, "classic-mod:0123456789abcdef", "  My MCM / Settings  ");
	MCMBridge::SetMCMAlias(settings, "classic-mod:other", "My MCM / Settings");
	REQUIRE(MCMBridge::SaveBridgeSettings(fixture.Path(), settings));
	auto loaded = MCMBridge::LoadBridgeSettings(fixture.Path());
	REQUIRE(loaded);
	CHECK(loaded->aliases.size() == 2);
	CHECK(MCMBridge::ResolveMCMAlias(*loaded, "classic-mod:0123456789abcdef", "Original") == "My MCM / Settings");
	MCMBridge::SetMCMAlias(*loaded, "classic-mod:0123456789abcdef", " \t ");
	REQUIRE(MCMBridge::SaveBridgeSettings(fixture.Path(), *loaded));
	loaded = MCMBridge::LoadBridgeSettings(fixture.Path());
	REQUIRE(loaded);
	CHECK(loaded->aliases.size() == 1);
	CHECK(MCMBridge::ResolveMCMAlias(*loaded, "classic-mod:0123456789abcdef", "Original") == "Original");
}

TEST_CASE("Aliases cannot inject ImGui IDs or control characters")
{
	MCMBridge::BridgeSettings settings;
	MCMBridge::SetMCMAlias(settings, "mod", "Name##hidden\nnext");
	CHECK(MCMBridge::ResolveMCMAlias(settings, "mod", "Original") == "Name  hidden next");
}

TEST_CASE("Provider aliases are not persisted as Bridge preferences")
{
	SettingsFixture           fixture;
	MCMBridge::BridgeSettings settings;
	settings.providerAliases["mod"] = "Unlocked alias";
	REQUIRE(MCMBridge::SaveBridgeSettings(fixture.Path(), settings));
	const auto loaded = MCMBridge::LoadBridgeSettings(fixture.Path());
	REQUIRE(loaded);
	CHECK(loaded->providerAliases.empty());
	CHECK(loaded->aliases.empty());
}

TEST_CASE("The shipped INI contains all Bridge defaults")
{
	const auto path = std::filesystem::path(MCM_BRIDGE_FIXTURE_DIR).parent_path().parent_path() /
	                  "config" / "MCMBridge.ini";
	REQUIRE(std::filesystem::is_regular_file(path));
	const auto settings = MCMBridge::LoadBridgeSettings(path);
	REQUIRE(settings);
	const MCMBridge::BridgeSettings defaults;
	CHECK(settings->pauseDuringWrites == defaults.pauseDuringWrites);
	CHECK(settings->closeJournalOnRedirect == defaults.closeJournalOnRedirect);
	CHECK(settings->groupMCMs == defaults.groupMCMs);
	std::ifstream     file(path);
	const std::string content{ std::istreambuf_iterator<char>(file), {} };
	CHECK(content.find("PauseDuringWrites=") != std::string::npos);
	CHECK(content.find("RedirectMCM=") == std::string::npos);
	CHECK(content.find("CloseJournalOnRedirect=") != std::string::npos);
	CHECK(content.find("GroupMCMs=") != std::string::npos);
	CHECK(content.find("AlphabeticMCMs=") != std::string::npos);
	CHECK(content.find("MCMRangeEnds=") != std::string::npos);
}

TEST_CASE("Alphabetical grouping persists and invalid range files use safe defaults")
{
	SettingsFixture           fixture;
	MCMBridge::BridgeSettings settings;
	settings.groupMCMs = true;
	settings.alphabeticMCMs = true;
	settings.mcmRangeEnds = "DFZ";
	REQUIRE(MCMBridge::SaveBridgeSettings(fixture.Path(), settings));
	const auto loaded = MCMBridge::LoadBridgeSettings(fixture.Path());
	REQUIRE(loaded);
	CHECK(loaded->groupMCMs);
	CHECK(loaded->alphabeticMCMs);
	CHECK(loaded->mcmRangeEnds == "DFZ");
	{
		std::ofstream file(fixture.Path());
		file << "[General]\nAlphabeticMCMs=true\nMCMRangeEnds=ZZ\n";
	}
	const auto invalid = MCMBridge::LoadBridgeSettings(fixture.Path());
	REQUIRE(invalid);
	CHECK(invalid->mcmRangeEnds == "CGLRZ");
}

TEST_CASE("MCM grouping persists independently without changing aliases")
{
	SettingsFixture           fixture;
	MCMBridge::BridgeSettings settings;
	settings.pauseDuringWrites = false;
	settings.aliases["original-id"] = "Alias / Name";
	for (const bool grouped : { true, false, true }) {
		settings.groupMCMs = grouped;
		REQUIRE(MCMBridge::SaveBridgeSettings(fixture.Path(), settings));
		const auto loaded = MCMBridge::LoadBridgeSettings(fixture.Path());
		REQUIRE(loaded);
		CHECK(loaded->groupMCMs == grouped);
		CHECK_FALSE(loaded->pauseDuringWrites);
		CHECK(loaded->aliases == settings.aliases);
	}
}

TEST_CASE("The pause preference persists while unrelated INI entries survive")
{
	SettingsFixture fixture;
	{
		std::ofstream file(fixture.Path());
		file << "[Unrelated]\nPreserve=original\n";
	}
	REQUIRE(MCMBridge::SaveBridgeSettings(fixture.Path(), { false }));
	const auto disabled = MCMBridge::LoadBridgeSettings(fixture.Path());
	REQUIRE(disabled);
	CHECK_FALSE(disabled->pauseDuringWrites);
	REQUIRE(MCMBridge::SaveBridgeSettings(fixture.Path(), { true }));
	const auto enabled = MCMBridge::LoadBridgeSettings(fixture.Path());
	REQUIRE(enabled);
	CHECK(enabled->pauseDuringWrites);
	std::ifstream     file(fixture.Path());
	const std::string content{ std::istreambuf_iterator<char>(file), {} };
	CHECK(content.find("Preserve = original") != std::string::npos);
}

TEST_CASE("Unwritable Bridge settings report an error without overwriting the target")
{
	SettingsFixture fixture;
	std::filesystem::create_directory(fixture.Path());
	const auto result = MCMBridge::SaveBridgeSettings(fixture.Path(), { false });
	REQUIRE_FALSE(result);
	CHECK(result.error().code == MCMBridge::BridgeErrorCode::kIoError);
	CHECK(std::filesystem::is_directory(fixture.Path()));
}

TEST_CASE("Journal closing preference persists independently of the pause preference")
{
	SettingsFixture fixture;
	for (const bool closeJournal : { false, true }) {
		for (const bool pause : { false, true }) {
			const MCMBridge::BridgeSettings values{ pause, closeJournal };
			REQUIRE(MCMBridge::SaveBridgeSettings(fixture.Path(), values));
			const auto loaded = MCMBridge::LoadBridgeSettings(fixture.Path());
			REQUIRE(loaded);
			CHECK(loaded->pauseDuringWrites == pause);
			CHECK(loaded->closeJournalOnRedirect == closeJournal);
		}
	}
}

TEST_CASE("Journal redirection only exposes the native frontend")
{
	using Action = MCMBridge::JournalRedirectAction;
	for (const bool close : { false, true }) {
		for (const bool available : { false, true }) {
			const MCMBridge::BridgeSettings settings{ true, close };
			CHECK(MCMBridge::ResolveJournalRedirect(settings, available) ==
				  (!available ? Action::kUnavailable : close ? Action::kCloseJournal :
															   Action::kKeepJournal));
		}
	}
}

TEST_CASE("Legacy redirect-disable value cannot disable the Journal redirect")
{
	SettingsFixture fixture;
	{
		std::ofstream file(fixture.Path());
		file << "[General]\nPauseDuringWrites=false\nRedirectMCM=false\nCloseJournalOnRedirect=true\n";
	}
	const auto settings = MCMBridge::LoadBridgeSettings(fixture.Path());
	REQUIRE(settings);
	CHECK_FALSE(settings->pauseDuringWrites);
	CHECK(settings->closeJournalOnRedirect);
	CHECK(MCMBridge::ResolveJournalRedirect(*settings, true) == MCMBridge::JournalRedirectAction::kCloseJournal);
	REQUIRE(MCMBridge::SaveBridgeSettings(fixture.Path(), *settings));
	std::ifstream     file(fixture.Path());
	const std::string content{ std::istreambuf_iterator<char>(file), {} };
	CHECK(content.find("RedirectMCM") == std::string::npos);
}

TEST_CASE("Journal handoff opens exactly once after its close event")
{
	MCMBridge::JournalHandoff handoff;
	const auto                request = handoff.Begin(10);
	REQUIRE(request != 0);
	CHECK(handoff.Begin(10) == 0);
	CHECK_FALSE(handoff.Complete(request, 11, false, false));
	CHECK(handoff.JournalClosed(11) == request);
	CHECK(handoff.JournalClosed(11) == 0);
	CHECK_FALSE(handoff.Expire(request));
	CHECK(handoff.Complete(request, 11, false, false));
	CHECK_FALSE(handoff.Complete(request, 11, false, false));
}

TEST_CASE("Journal handoff ignores late close events after timeout")
{
	MCMBridge::JournalHandoff handoff;
	const auto                request = handoff.Begin(10);
	CHECK(handoff.Expire(request));
	CHECK_FALSE(handoff.Expire(request));
	CHECK(handoff.JournalClosed(11) == 0);
	CHECK_FALSE(handoff.Complete(request, 11, false, false));
}

TEST_CASE("Journal handoff cancels queued completion on reopen or game transition")
{
	MCMBridge::JournalHandoff handoff;
	const auto                request = handoff.Begin(10);
	REQUIRE(handoff.JournalClosed(11) == request);
	handoff.Cancel();
	CHECK_FALSE(handoff.Complete(request, 11, false, false));
	CHECK_FALSE(handoff.Expire(request));
}

TEST_CASE("An old handoff timer cannot cancel a newer request")
{
	MCMBridge::JournalHandoff handoff;
	const auto                previous = handoff.Begin(10);
	handoff.Cancel();
	const auto current = handoff.Begin(12);
	REQUIRE(current != previous);
	CHECK_FALSE(handoff.Expire(previous));
	CHECK(handoff.JournalClosed(13) == current);
	CHECK(handoff.Complete(current, 13, false, false));
}

TEST_CASE("Journal handoff validates the final menu state without retrying")
{
	MCMBridge::JournalHandoff handoff;
	const auto                request = handoff.Begin(10);
	REQUIRE(handoff.JournalClosed(11) == request);
	SECTION("Journal reopened") { CHECK_FALSE(handoff.Complete(request, 12, true, false)); }
	SECTION("Journal still open") { CHECK_FALSE(handoff.Complete(request, 11, true, false)); }
	SECTION("Game unavailable") { CHECK_FALSE(handoff.Complete(request, 11, false, true)); }
	CHECK_FALSE(handoff.Complete(request, 11, false, false));
}

TEST_CASE("Bridge aliases override provider aliases without changing original names")
{
	using namespace MCMBridge;
	BridgeSettings    settings;
	const std::string original = "Original";
	settings.providerAliases["stable-id"] = "Provider alias";
	CHECK(ResolveMCMAlias(settings, "stable-id", original) == "Provider alias");
	SetMCMAlias(settings, "stable-id", "Bridge alias");
	CHECK(ResolveMCMAlias(settings, "stable-id", original) == "Bridge alias");
	SetMCMAlias(settings, "stable-id", "");
	CHECK(ResolveMCMAlias(settings, "stable-id", original) == "Provider alias");
	settings.providerAliases.clear();
	CHECK(ResolveMCMAlias(settings, "stable-id", original) == "Original");
	CHECK(original == "Original");
}
