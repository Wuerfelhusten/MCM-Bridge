#include "MCMBridge/Core/BridgeSettings.h"
#include "MCMBridge/Core/FrameworkMenuPath.h"
#include "MCMBridge/Core/MCMOrganization.h"

#include <catch2/catch_test_macros.hpp>
#include <vector>

TEST_CASE("Alphabetical ranges cover A-Z without gaps or overlaps")
{
	using namespace MCMBridge;
	CHECK(ValidMCMRanges("CGLRZ"));
	CHECK(ValidMCMRanges("Z"));
	CHECK(ValidMCMRanges("ABCDEFGHIJKLMNOPQRSTUVWXYZ"));
	for (const auto invalid : { "", "ABC", "CCZ", "GCZ", "cz", "[", "@Z" }) CHECK_FALSE(ValidMCMRanges(invalid));
	CHECK(MCMNameRange("Alpha", "CGLRZ") == "A-C");
	CHECK(MCMNameRange("conduit", "CGLRZ") == "A-C");
	CHECK(MCMNameRange("Delta", "CGLRZ") == "D-G");
	CHECK(MCMNameRange("Legacy", "CGLRZ") == "H-L");
	CHECK(MCMNameRange("zTest", "CGLRZ") == "S-Z");
	CHECK(MCMNameRange("123", "CGLRZ") == "0-9 & Other");
	CHECK(MCMNameRange("", "CGLRZ") == "0-9 & Other");
	CHECK(MCMNameRange("$Untranslated", "CGLRZ") == "0-9 & Other");
	CHECK(MCMNameRange("\xC3\x84nderung", "CGLRZ") == "A-C");
	CHECK(MCMNameRange("\xC3\xB6ption", "CGLRZ") == "M-R");
	CHECK(MCMNameRange("\xE4\xB8\xAD", "CGLRZ") == "0-9 & Other");
}

TEST_CASE("Balanced folders minimize population differences without splitting letters")
{
	using namespace MCMBridge;
	const std::vector<std::string> names{ "Alpha", "Another", "Beta", "Bravo", "Yankee", "Yellow", "Zulu", "Zebra" };
	const auto                     ranges = BalanceMCMRanges(names, 2);
	REQUIRE(ValidMCMRanges(ranges));
	REQUIRE(ranges.size() == 2);
	int first{};
	for (const auto& name : names)
		if (MCMNameRange(name, ranges) == MCMNameRange("A", ranges))
			++first;
	CHECK(first == 4);
	CHECK(MCMNameRange("Alpha", ranges) == MCMNameRange("Another", ranges));
	for (int count = 1; count <= 26; ++count) {
		const auto empty = BalanceMCMRanges({}, count);
		CHECK(ValidMCMRanges(empty));
		CHECK(empty.size() == static_cast<std::size_t>(count));
		const auto populated = BalanceMCMRanges(names, count);
		CHECK(ValidMCMRanges(populated));
		CHECK(populated.size() == static_cast<std::size_t>(count));
	}
	CHECK(BalanceMCMRanges(names, 0) == "Z");
	CHECK(BalanceMCMRanges(names, 99).size() == 26);
}

TEST_CASE("Alias changes move presentation ranges without changing source names or IDs")
{
	using namespace MCMBridge;
	BridgeSettings    settings;
	const std::string id = "stable-original-id";
	const std::string original = "Alpha";
	const auto        before = MCMNameRange(ResolveMCMAlias(settings, id, original), settings.mcmRangeEnds);
	SetMCMAlias(settings, id, "Zulu / Example");
	const auto name = ResolveMCMAlias(settings, id, original);
	const auto after = MCMNameRange(name, settings.mcmRangeEnds);
	CHECK(before == "A-C");
	CHECK(after == "S-Z");
	CHECK(original == "Alpha");
	CHECK(MCMFrameworkSectionPath(name, true, after) == "MCMs/S-Z/Zulu \\/ Example");
	CHECK(MCMFrameworkSectionPath(name, false, after) == "Zulu \\/ Example");
	CHECK(JoinFrameworkMenuPath(name, "Page / One", true, after) == "MCMs/S-Z/Zulu \\/ Example/Page \\/ One");
	SetMCMAlias(settings, id, {});
	CHECK(MCMNameRange(ResolveMCMAlias(settings, id, original), settings.mcmRangeEnds) == before);
}

TEST_CASE("Framework menu labels are readable and unique within each folder")
{
	using namespace MCMBridge;
	std::unordered_set<std::string> used;
	CHECK(UniqueFrameworkMenuLabel("Frostfall", used) == "Frostfall");
	CHECK(UniqueFrameworkMenuLabel("Frostfall", used) == "Frostfall (2)");
	CHECK(UniqueFrameworkMenuLabel("Frostfall (2)", used) == "Frostfall (2) (2)");
	CHECK(UniqueFrameworkMenuLabel("Frostfall", used) == "Frostfall (3)");
	CHECK(MCMFrameworkSectionPath("Frostfall (3)", true, "D-G") == "MCMs/D-G/Frostfall (3)");
}

TEST_CASE("Root MCM labels capitalize the initial without changing stored aliases")
{
	using namespace MCMBridge;
	CHECK(UppercaseMCMRootInitial("precision") == "Precision");
	CHECK(UppercaseMCMRootInitial("  take Notes!") == "  Take Notes!");
	CHECK(UppercaseMCMRootInitial("123 choices") == "123 choices");
	CHECK(UppercaseMCMRootInitial("\xC3\xA4nderung") == "\xC3\x84nderung");
	CHECK(UppercaseMCMRootInitial("\xC3\x9F"
								  "child") ==
		  "\xE1\xBA\x9E"
		  "child");
	CHECK(UppercaseMCMRootInitial("Frostfall") == "Frostfall");
}
