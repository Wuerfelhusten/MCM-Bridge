#include "MCMBridge/Papyrus/HostCallArguments.h"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <limits>

TEST_CASE("Host call arguments are validated and copied without changing requested values")
{
	std::string      page = "Raw/Page";
	const std::array arguments{ MCMHostArgument{ MCM_HOST_STRING, 0, 0, page.c_str() }, MCMHostArgument{ MCM_HOST_INTEGER, 3, 0, nullptr } };
	MCMHostCall      call{ "Script::Menu", "SetPage", arguments.data(), 2, 10000, 0 };
	const auto       decoded = MCMBridge::DecodeHostCall(call);
	REQUIRE(decoded);
	page = "changed";
	CHECK(decoded->text == "Raw/Page");
	CHECK(decoded->integer == 3);
	CHECK(decoded->method == MCMBridge::ClassicMethod::kSetPage);
	call.argument_count = 1;
	CHECK_FALSE(MCMBridge::DecodeHostCall(call));
	call.argument_count = 2;
	call.arguments = nullptr;
	CHECK_FALSE(MCMBridge::DecodeHostCall(call));
}

TEST_CASE("Host calls preserve Helper callbacks and reject unknown functions")
{
	const std::array arguments{ MCMHostArgument{ MCM_HOST_STRING, 0, 0, "iKey" }, MCMHostArgument{ MCM_HOST_INTEGER, 42, 0, nullptr } };
	MCMHostCall      call{ "Script::Menu", "SetModSettingInt", arguments.data(), 2, 10000, 0 };
	const auto       setting = MCMBridge::DecodeHostCall(call);
	REQUIRE(setting);
	CHECK(setting->method == MCMBridge::ClassicMethod::kSetModSettingInt);
	CHECK(setting->text == "iKey");
	CHECK(setting->integer == 42);
	call.function = "OnSettingChange";
	call.argument_count = 1;
	REQUIRE(MCMBridge::DecodeHostCall(call));
	call.function = "Unknown";
	CHECK_FALSE(MCMBridge::DecodeHostCall(call));
}

TEST_CASE("Host slider transport rejects nonfinite values but does not normalize input")
{
	MCMHostArgument argument{ MCM_HOST_FLOAT, 0, 1.125F, nullptr };
	MCMHostCall     call{ "Script::Menu", "SetSliderValue", &argument, 1, 10000, 0 };
	const auto      decoded = MCMBridge::DecodeHostCall(call);
	REQUIRE(decoded);
	CHECK(decoded->number == 1.125F);
	argument.number = std::numeric_limits<float>::quiet_NaN();
	CHECK_FALSE(MCMBridge::DecodeHostCall(call));
	argument.number = std::numeric_limits<float>::infinity();
	CHECK_FALSE(MCMBridge::DecodeHostCall(call));
}
