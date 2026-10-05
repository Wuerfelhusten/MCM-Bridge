#include "MCMBridge/Core/ExternalOperationState.h"

#include <catch2/catch_test_macros.hpp>

using namespace MCMBridge;

TEST_CASE("External admission drains existing UI work before granting ownership")
{
	ExternalOperationState state;
	std::uint64_t          token{};
	CHECK(state.Begin("Memory", false, true, token) == ExternalAdmission::kBusy);
	CHECK_FALSE(state.BlocksNormalWork());
	CHECK(state.Begin("Memory", true, false, token) == ExternalAdmission::kBusy);
	CHECK(state.IsWaitingFor("Memory"));
	CHECK(state.BlocksNormalWork());
	CHECK(state.Begin("Other", false, false, token) == ExternalAdmission::kBusy);
	CHECK_FALSE(state.Cancel("Other"));
	CHECK(state.Begin("Memory", false, false, token) == ExternalAdmission::kReady);
	REQUIRE(token != 0);
	CHECK(state.Cancel("Memory"));
	CHECK(state.BlocksNormalWork());
	CHECK_FALSE(state.End(token + 1));
	CHECK(state.End(token));
	CHECK_FALSE(state.BlocksNormalWork());
}

TEST_CASE("Cancelled waiting admission and invalidated sessions cannot retain ownership")
{
	ExternalOperationState state;
	std::uint64_t          token{};
	CHECK(state.Begin("", false, false, token) == ExternalAdmission::kInvalid);
	CHECK(state.Begin("Memory", true, false, token) == ExternalAdmission::kBusy);
	CHECK(state.Cancel("Memory"));
	CHECK_FALSE(state.BlocksNormalWork());
	REQUIRE(state.Begin("Memory", false, false, token) == ExternalAdmission::kReady);
	const auto previous = token;
	state.Reset();
	CHECK_FALSE(state.End(previous));
	REQUIRE(state.Begin("Memory", false, false, token) == ExternalAdmission::kReady);
	CHECK(token != previous);
	CHECK_FALSE(state.End(previous));
	CHECK(state.End(token));
}
