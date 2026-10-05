#include "MCMBridge/Core/FacadeCallState.h"
#include "MCMBridge/Core/NativeHostSession.h"
#include "MCMBridge/Core/ViewLoadState.h"

#include <catch2/catch_test_macros.hpp>

using namespace MCMBridge;

TEST_CASE("Borrowed page suspension cannot overwrite a newer frontend selection")
{
	ViewLoadState view;
	const auto    borrowed = view.Request("mod", "General");
	REQUIRE(view.Complete(borrowed));
	REQUIRE(view.Suspend(borrowed));
	CHECK_FALSE(view.Ready("mod", "General"));
	CHECK(view.Error().empty());
	CHECK(view.Current(borrowed));
	const auto selected = view.Request("mod", "Advanced");
	REQUIRE(view.Complete(selected));
	CHECK_FALSE(view.Suspend(borrowed));
	CHECK_FALSE(view.Complete(borrowed));
	CHECK_FALSE(view.Remap(borrowed, "Script redirect"));
	CHECK(view.Ready("mod", "Advanced"));
}

TEST_CASE("External facade adopts an existing config without reopening or replacing ownership")
{
	FacadeCallState state;
	REQUIRE_FALSE(state.Adopt(0, 10, 20));
	REQUIRE(state.Adopt(1, 10, 20));
	REQUIRE(state.IsOpen());
	REQUIRE_FALSE(state.Adopt(1, 11, 21));
	REQUIRE_FALSE(state.Enter(1, 10, 30, FacadeCallKind::kOpen));
	REQUIRE_FALSE(state.Enter(2, 10, 30, FacadeCallKind::kPage));
	REQUIRE_FALSE(state.Enter(1, 11, 30, FacadeCallKind::kPage));
	const auto call = state.Enter(1, 10, 30, FacadeCallKind::kPage);
	REQUIRE(call);
	REQUIRE_FALSE(state.Enter(1, 10, 31, FacadeCallKind::kPage));
	REQUIRE(state.Leave(*call, 30));
	CHECK(state.Token() == 20);
	state.Reset();
	REQUIRE(state.Adopt(1, 10, 20));
	const auto next = state.Enter(1, 10, 31, FacadeCallKind::kOperation);
	REQUIRE(next);
	CHECK(*next != *call);
	REQUIRE_FALSE(state.Leave(*call, 30));
	REQUIRE(state.Leave(*next, 31));
}

TEST_CASE("External facade calls finish in stack order before returning page data")
{
	FacadeCallState state;
	REQUIRE(state.Open(1, 10, 20));
	REQUIRE_FALSE(state.Enter(1, 10, 30, FacadeCallKind::kPage));
	const auto opening = state.Enter(1, 10, 30, FacadeCallKind::kOpen);
	REQUIRE(opening);
	const auto page = state.Enter(1, 10, 30, FacadeCallKind::kPage);
	REQUIRE(page);
	REQUIRE_FALSE(state.Enter(1, 10, 31, FacadeCallKind::kOperation));
	REQUIRE_FALSE(state.Enter(1, 10, 30, FacadeCallKind::kClose));
	REQUIRE_FALSE(state.Leave(*opening, 30));
	REQUIRE_FALSE(state.Leave(*page, 31));
	REQUIRE(state.Leave(*page, 30));
	REQUIRE_FALSE(state.IsOpen());
	REQUIRE(state.Leave(*opening, 30));
	REQUIRE(state.IsOpen());
	REQUIRE_FALSE(state.Busy());
	REQUIRE_FALSE(state.Enter(1, 10, 30, FacadeCallKind::kOpen));
	const auto close = state.Enter(1, 10, 31, FacadeCallKind::kClose);
	REQUIRE(close);
	REQUIRE_FALSE(state.Enter(1, 10, 31, FacadeCallKind::kPage));
	REQUIRE(state.Leave(*close, 31));
	REQUIRE_FALSE(state.Owns(1, 10));
}

TEST_CASE("External facade timeout and save invalidation revoke permits without reuse")
{
	FacadeCallState state;
	REQUIRE(state.Open(1, 10, 20));
	const auto old = state.Enter(1, 10, 30, FacadeCallKind::kOpen);
	REQUIRE(old);
	state.Reset();
	REQUIRE_FALSE(state.Running(*old));
	REQUIRE(state.Open(2, 10, 21));
	REQUIRE_FALSE(state.Enter(1, 10, 30, FacadeCallKind::kOpen));
	REQUIRE_FALSE(state.Enter(2, 11, 30, FacadeCallKind::kOpen));
	const auto current = state.Enter(2, 10, 30, FacadeCallKind::kOpen);
	REQUIRE(current);
	REQUIRE(*current != *old);
	REQUIRE_FALSE(state.Leave(*old, 30));
	REQUIRE(state.Running(*current));
}

TEST_CASE("External facade serial calls keep one config across 2233 settings")
{
	FacadeCallState state;
	REQUIRE(state.Open(1, 10, 20));
	const auto open = state.Enter(1, 10, 0, FacadeCallKind::kOpen);
	REQUIRE(open);
	REQUIRE(state.Leave(*open, 0));
	for (std::uint32_t index = 1; index <= 2233; ++index) {
		const auto call = state.Enter(1, 10, index, FacadeCallKind::kOperation);
		REQUIRE(call);
		REQUIRE_FALSE(state.Enter(1, 10, index + 1, FacadeCallKind::kPage));
		REQUIRE(state.Leave(*call, index));
		REQUIRE(state.Token() == 20);
	}
}

TEST_CASE("External page completion exposes one finished transaction without reopening config")
{
	NativeHostSession host;
	host.Reset(1);
	const auto token = host.Open(1, "fixture", 10);
	REQUIRE(token);
	FacadeCallState calls;
	REQUIRE(calls.Open(1, 10, *token));
	const auto opening = calls.Enter(1, 10, 30, FacadeCallKind::kOpen);
	REQUIRE(opening);
	REQUIRE(calls.Leave(*opening, 30));
	for (std::int32_t page = 0; page < 3; ++page) {
		const auto admission = calls.Enter(1, 10, 31, FacadeCallKind::kPage);
		REQUIRE(admission);
		REQUIRE(host.BeginPage(*token, "Page" + std::to_string(page), page));
		const auto id = host.AddOption(*token, 3, "Toggle", "", 1.0F, 0, "Enabled");
		REQUIRE(id >= 0);
		REQUIRE(calls.Busy());
		REQUIRE_FALSE(calls.Enter(1, 10, 32, FacadeCallKind::kOperation));
		REQUIRE(host.Publish(*token));
		REQUIRE(calls.Leave(*admission, 31));
		const auto result = host.Read();
		REQUIRE(result);
		REQUIRE(result->index == page);
		REQUIRE(result->buffers.numericValues.at(0) == 1.0F);
		REQUIRE(result->buffers.stateNames.at(0) == "Enabled");
		REQUIRE(host.IsActive(*token));
	}
}

TEST_CASE("External facade nesting is bounded and revoked calls cannot finish a later config")
{
	FacadeCallState state;
	REQUIRE(state.Open(1, 10, 20));
	std::vector<std::uint64_t> permits;
	const auto                 opening = state.Enter(1, 10, 30, FacadeCallKind::kOpen);
	REQUIRE(opening);
	permits.push_back(*opening);
	for (int depth = 1; depth < 64; ++depth) {
		const auto permit = state.Enter(1, 10, 30, FacadeCallKind::kPage);
		REQUIRE(permit);
		permits.push_back(*permit);
	}
	REQUIRE_FALSE(state.Enter(1, 10, 30, FacadeCallKind::kPage));
	state.Reset();
	REQUIRE(state.Open(1, 10, 21));
	const auto replacement = state.Enter(1, 10, 30, FacadeCallKind::kOpen);
	REQUIRE(replacement);
	for (const auto permit : permits) {
		REQUIRE_FALSE(state.Leave(permit, 30));
		REQUIRE_FALSE(state.Running(permit));
	}
	REQUIRE(state.Leave(*replacement, 30));
}
