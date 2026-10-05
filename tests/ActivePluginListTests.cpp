#include "MCMBridge/Core/ActivePluginList.h"

#include <catch2/catch_test_macros.hpp>
#include <sstream>

using namespace MCMBridge;

TEST_CASE("Plugin activation uses exact case-insensitive starred entries")
{
	for (const auto text : { "*McmRecorder.esp", "\xEF\xBB\xBF# Header\r\n*mCMrECORDER.ESP\r\n",
			 "Other.esp\r  *McmRecorder.esp \t\r", "*McmRecorder.esp\n*McmRecorder.esp" }) {
		std::istringstream input(text);
		REQUIRE(IsPluginActive(input, "McmRecorder.esp") == true);
	}
	for (const auto text : { "", "McmRecorder.esp", "#*McmRecorder.esp\n*Other.esp",
			 "*NotMcmRecorder.esp\n*McmRecorder.esp.backup" }) {
		std::istringstream input(text);
		REQUIRE(IsPluginActive(input, "McmRecorder.esp") == false);
	}
}

TEST_CASE("Unreadable or ambiguous plugin lists require a fallback")
{
	for (const auto& text : { std::string("*McmRecorder.esp\nMcmRecorder.esp"),
			 std::string("\xFF\xFE"), std::string("abc\0def", 7), std::string(4 * 1024 * 1024 + 1, 'x') }) {
		std::istringstream input(text);
		REQUIRE_FALSE(IsPluginActive(input, "McmRecorder.esp").has_value());
	}
	std::istringstream input("*McmRecorder.esp");
	input.setstate(std::ios::badbit);
	REQUIRE_FALSE(IsPluginActive(input, "McmRecorder.esp").has_value());
}

TEST_CASE("Plugin list folder follows runtime distribution")
{
	constexpr auto runtime = (1U << 24U) | (6U << 16U) | (1170U << 4U);
	REQUIRE(PluginListFolder(runtime) == "Skyrim Special Edition");
	REQUIRE(PluginListFolder(runtime | 1U) == "Skyrim Special Edition GOG");
	REQUIRE(PluginListFolder(runtime | 2U) == "Skyrim Special Edition EPIC");
	REQUIRE_FALSE(PluginListFolder(runtime | 3U).has_value());
	REQUIRE(PluginListFolder(659U << 4U) == "Skyrim Special Edition GOG");
	REQUIRE(PluginListFolder(1179U << 4U) == "Skyrim Special Edition GOG");
	REQUIRE(PluginListFolder(678U << 4U) == "Skyrim Special Edition EPIC");
}
