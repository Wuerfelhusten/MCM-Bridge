#include "MCMBridge/Core/MCMHelperParser.h"
#include "MCMBridge/Core/MCMHelperValidation.h"

#include <catch2/catch_test_macros.hpp>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <limits>

namespace
{
	nlohmann::json Config()
	{
		return nlohmann::json::parse(R"({
			"modName": "VersionFixture",
			"displayName": "Version Fixture",
			"content": [{"type": "toggle", "text": "Enabled"}]
		})");
	}
}

TEST_CASE("MCM Helper version validation accepts legacy and absent requirements")
{
	auto document = Config();
	CHECK(MCMBridge::ValidateMCMHelperConfig(document));
	for (const auto& version : nlohmann::json::array({ 0, 1, 9, 11, 12, 13, 14, (std::numeric_limits<std::uint32_t>::max)() })) {
		INFO(version.dump());
		document["minMcmVersion"] = version;
		CHECK(MCMBridge::ValidateMCMHelperConfig(document));
	}
}

TEST_CASE("MCM Helper version validation rejects wrong types and out of range numbers")
{
	auto document = Config();
	for (const auto& version : nlohmann::json::array({ nullptr, true, "9", 9.0, -1, nlohmann::json::array(), nlohmann::json::object(),
			 (std::numeric_limits<std::int64_t>::min)(),
			 (std::numeric_limits<std::int64_t>::max)(),
			 std::uint64_t{ 4294967296 },
			 (std::numeric_limits<std::uint64_t>::max)() })) {
		INFO(version.dump());
		document["minMcmVersion"] = version;
		auto result = MCMBridge::ValidateMCMHelperConfig(document);
		REQUIRE_FALSE(result);
		CHECK(result.error().code == MCMBridge::BridgeErrorCode::kInvalidData);
	}
}

TEST_CASE("MCM Helper parser preserves legacy minimum versions and missing requirements")
{
	const auto root = std::filesystem::path(MCM_BRIDGE_FIXTURE_DIR) / "MCMHelper";
	SECTION("Version 9 retains all control metadata")
	{
		auto result = MCMBridge::MCMHelperParser{}.Parse({ .config = root / "MCM" / "Config" / "MCMBridgeFixture" / "config.json" });
		REQUIRE(result);
		CHECK(result->minimumMCMHelperVersion == 9);
		REQUIRE(result->pages.size() == 1);
		CHECK(result->pages.front().controls.size() == 11);
	}
	SECTION("Version 11 retains menu choices")
	{
		auto result = MCMBridge::MCMHelperParser{}.Parse({ .config = root / "legacy-config.json" });
		REQUIRE(result);
		CHECK(result->minimumMCMHelperVersion == 11);
		REQUIRE(result->pages.size() == 1);
		REQUIRE(result->pages.front().controls.size() == 1);
		const auto& control = result->pages.front().controls.front();
		CHECK(control.identity.explicitID == "iMode:General");
		REQUIRE(control.menu);
		CHECK(control.menu->options == std::vector<std::string>{ "First", "Second", "Third" });
	}
	SECTION("No declared minimum stays unspecified")
	{
		auto result = MCMBridge::MCMHelperParser{}.Parse({ .config = root / "custom-config.json" });
		REQUIRE(result);
		CHECK(result->minimumMCMHelperVersion == 0);
	}
}
