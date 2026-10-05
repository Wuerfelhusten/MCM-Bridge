#include "MCMBridge/Core/HostPageCapture.h"

#include <catch2/catch_test_macros.hpp>

using namespace MCMBridge;

namespace
{
	void Fill(HostPageCapture& a_capture, HostCaptureToken a_token)
	{
		REQUIRE(a_capture.Observe(a_token, "setOptionFlagsBuffer", std::vector<std::int32_t>{ 3, 0 }));
		REQUIRE(a_capture.Observe(a_token, "setOptionTextBuffer", std::vector<std::string>{ "Enabled", "" }));
		REQUIRE(a_capture.Observe(a_token, "setOptionStrValueBuffer", std::vector<std::string>{ "", "" }));
		REQUIRE(a_capture.Observe(a_token, "setOptionNumValueBuffer", std::vector<float>{ 1, 0 }));
	}
}

TEST_CASE("Native host publishes a complete flushed page only", "[host]")
{
	HostPageCapture capture;
	const auto      token = capture.Begin(1, "General/Advanced", 2);
	Fill(capture, token);
	REQUIRE(capture.Observe(token, "flushOptionBuffers", std::int32_t{ 1 }));
	const auto result = capture.Complete(token);
	REQUIRE(result);
	CHECK(result->page == "General/Advanced");
	CHECK(result->index == 2);
	CHECK(result->buffers.optionFlags == std::vector<std::int32_t>{ 3 });
	CHECK(result->buffers.numericValues == std::vector<float>{ 1 });
	CHECK(result->buffers.stateNames.empty());
	CHECK_FALSE(capture.Complete(token));
}

TEST_CASE("Native host rejects incomplete and malformed captures", "[host]")
{
	HostPageCapture capture;
	const auto      token = capture.Begin(1, "", -1);
	Fill(capture, token);
	SECTION("Missing flush") {}
	SECTION("Negative count") { capture.Observe(token, "flushOptionBuffers", std::int32_t{ -1 }); }
	SECTION("Oversized count") { capture.Observe(token, "flushOptionBuffers", std::int32_t{ 3 }); }
	SECTION("Wrong payload")
	{
		capture.Observe(token, "setOptionTextBuffer", std::int32_t{ 1 });
		capture.Observe(token, "flushOptionBuffers", std::int32_t{ 1 });
	}
	SECTION("Partial second page")
	{
		capture.Observe(token, "flushOptionBuffers", std::int32_t{ 1 });
		capture.Observe(token, "setOptionFlagsBuffer", std::vector<std::int32_t>{ 3 });
	}
	CHECK_FALSE(capture.Complete(token));
}

TEST_CASE("Native host isolates operation and session tokens", "[host]")
{
	HostPageCapture capture;
	const auto      old = capture.Begin(1, "Old", 0);
	Fill(capture, old);
	const auto current = capture.Begin(2, "New", 1);
	CHECK_FALSE(capture.Observe(old, "flushOptionBuffers", std::int32_t{ 1 }));
	CHECK_FALSE(capture.Complete(old));
	Fill(capture, current);
	capture.Observe(current, "flushOptionBuffers", std::int32_t{ 0 });
	const auto result = capture.Complete(current);
	REQUIRE(result);
	CHECK(result->buffers.optionFlags.empty());
	CHECK(result->page == "New");
	const auto cancelled = capture.Begin(2, "Cancelled", 3);
	capture.Cancel();
	CHECK_FALSE(capture.Observe(cancelled, "flushOptionBuffers", std::int32_t{ 0 }));
}

TEST_CASE("Native host ignores unrelated protocol methods", "[host]")
{
	HostPageCapture capture;
	const auto      token = capture.Begin(1, "Page", 0);
	CHECK_FALSE(capture.Observe(token, "other.flushOptionBuffers", std::int32_t{ 0 }));
	Fill(capture, token);
	capture.Observe(token, "flushOptionBuffers", std::int32_t{ 1 });
	CHECK(capture.Complete(token));
}
