#include "MCMBridge/Core/ClassicPageList.h"

#include <catch2/catch_test_macros.hpp>

using namespace MCMBridge;

TEST_CASE("Classic page navigation skips spacer and unused slots without renumbering")
{
	const std::vector<std::string> names{ "First", "", "Second", " \t", "", "Last" };
	const auto                     pages = BuildClassicPageList(names);
	REQUIRE(pages.size() == 3);
	CHECK(pages[0] == ClassicPageSelection{ "First", 0 });
	CHECK(pages[1] == ClassicPageSelection{ "Second", 2 });
	CHECK(pages[2] == ClassicPageSelection{ "Last", 5 });
	CHECK(names[1].empty());
	CHECK(names[3] == " \t");
}

TEST_CASE("Classic page navigation follows the SkyUI None sentinel")
{
	const std::vector<std::string> names{ "None", "nOnE", "NONE", "General", "$None", " None " };
	const auto                     pages = BuildClassicPageList(names);
	REQUIRE(pages.size() == 3);
	CHECK(pages[0] == ClassicPageSelection{ "General", 3 });
	CHECK(pages[1] == ClassicPageSelection{ "$None", 4 });
	CHECK(pages[2] == ClassicPageSelection{ " None ", 5 });
}

TEST_CASE("Classic page navigation preserves raw keys and legitimate duplicate names")
{
	const std::vector<std::string> names{ "$General", "Room (1/3)", "General", "General", "Wind " };
	const auto                     pages = BuildClassicPageList(names);
	REQUIRE(pages.size() == names.size());
	for (std::size_t index = 0; index < names.size(); ++index) {
		CHECK(pages[index].name == names[index]);
		CHECK(pages[index].index == static_cast<std::int32_t>(index));
	}
}

TEST_CASE("Classic menus without named pages keep one default page")
{
	const std::vector<std::string> names{ "", " \t\r\n", "None" };
	CHECK(BuildClassicPageList(names) == std::vector<ClassicPageSelection>{ { "", -1 } });
	CHECK(BuildClassicPageList({}) == std::vector<ClassicPageSelection>{ { "", -1 } });
}

TEST_CASE("Page scoped scans retain their valid opening page before other named pages")
{
	const std::vector<std::string> names{ "", "First", "None", "Second", "" };
	const auto                     pages = BuildClassicPageList(names, ClassicPageSelection{ "Second", 3 });
	REQUIRE(pages.size() == 2);
	CHECK(pages[0] == ClassicPageSelection{ "Second", 3 });
	CHECK(pages[1] == ClassicPageSelection{ "First", 1 });
}

TEST_CASE("Page scoped scans preserve the unnamed landing page but omit array spacers")
{
	const std::vector<std::string> names{ "", "First", "\t" };
	const auto                     pages = BuildClassicPageList(names, ClassicPageSelection{ "", -1 });
	REQUIRE(pages.size() == 2);
	CHECK(pages[0] == ClassicPageSelection{ "", -1 });
	CHECK(pages[1] == ClassicPageSelection{ "First", 1 });
	CHECK(BuildClassicPageList(names, ClassicPageSelection{ "", 0 }) ==
		  std::vector<ClassicPageSelection>{ { "First", 1 } });
}
