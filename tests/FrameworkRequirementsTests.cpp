#include "MCMBridge/Framework/FrameworkRequirements.h"

#include <catch2/catch_test_macros.hpp>

using namespace MCMBridge;

TEST_CASE("Framework admission uses host capabilities rather than legacy version numbers", "[framework]")
{
	CHECK(FrameworkRequirements::minimumRelease == "3.18");
	CHECK_FALSE(FrameworkRequirements::SupportsApi(0));
	CHECK(FrameworkRequirements::SupportsApi(1));
	CHECK(FrameworkRequirements::SupportsApi(2));
	CHECK(FrameworkRequirements::FindMissingExport([](const char*) { return true; }) == nullptr);
}

TEST_CASE("Every required Framework export must be available before registration", "[framework]")
{
	for (const auto missing : FrameworkRequirements::requiredExports) {
		const auto result = FrameworkRequirements::FindMissingExport([missing](const char* a_name) { return std::string_view(a_name) != missing; });
		REQUIRE(result != nullptr);
		CHECK(std::string_view(result) == missing);
	}
}
