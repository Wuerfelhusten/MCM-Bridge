#include "MCMBridge/Core/NativeMCMRegistry.h"
#include "MCMBridge/Core/NativeRegistrationRequests.h"

#include <catch2/catch_test_macros.hpp>
#include <vector>

using namespace MCMBridge;

TEST_CASE("Native registration admission has no fixed outstanding menu limit")
{
	NativeRegistrationRequests requests;
	requests.Reset(1);
	std::vector<std::int32_t> ids;
	ids.reserve(20000);
	for (std::uint32_t owner = 0; owner < 20000; ++owner) {
		const auto id = requests.Submit(1, owner);
		REQUIRE(id > 0);
		if (!ids.empty())
			CHECK(id > ids.back());
		ids.push_back(id);
	}
	for (std::uint32_t owner = 0; owner < ids.size(); ++owner) {
		if (owner % 2 == 0) {
			requests.Cancel(1, owner, ids[owner]);
			CHECK_FALSE(requests.Claim(1, ids[owner]));
		} else {
			REQUIRE(requests.Claim(1, ids[owner]));
			REQUIRE(requests.Complete(1, ids[owner], static_cast<std::int32_t>(owner)));
			CHECK(requests.Take(1, owner, ids[owner]) == static_cast<std::int32_t>(owner));
		}
		CHECK(requests.Take(1, owner, ids[owner]) == -1);
	}
	requests.Reset(2);
	CHECK(requests.Submit(2, 1) > ids.back());
	CHECK_FALSE(requests.Claim(1, ids.back()));
}

TEST_CASE("Grouped native registrations keep results pending until commit and discard cancelled members")
{
	NativeRegistrationRequests requests;
	NativeMCMRegistry          registry;
	const auto                 session = registry.BeginSession();
	requests.Reset(session);
	std::vector<std::int32_t>   ids;
	std::vector<NativeMCMEntry> entries;
	std::vector<std::size_t>    accepted;
	for (std::uint32_t index = 0; index < 2233; ++index) {
		ids.push_back(requests.Submit(session, index));
		REQUIRE(ids.back() > 0);
		if (index % 7 == 0)
			requests.Cancel(session, index, ids.back());
	}
	for (std::uint32_t index = 0; index < ids.size(); ++index) {
		if (!requests.Claim(session, ids[index]))
			continue;
		accepted.push_back(index);
		entries.push_back({ index + 1, std::to_string(index), "Menu" });
		CHECK(requests.Take(session, index, ids[index]) == NativeRegistrationRequests::kPending);
	}
	const auto slots = registry.RegisterBatch(session, std::move(entries));
	REQUIRE(slots);
	REQUIRE(slots->size() == accepted.size());
	for (std::size_t index = 0; index < accepted.size(); ++index) {
		const auto owner = static_cast<std::uint32_t>(accepted[index]);
		CHECK(requests.Take(session, owner, ids[owner]) == NativeRegistrationRequests::kPending);
		REQUIRE(registry.Find(session, (*slots)[index]));
		REQUIRE(requests.Complete(session, ids[owner], (*slots)[index]));
		CHECK(requests.Take(session, owner, ids[owner]) == (*slots)[index]);
		CHECK_FALSE(requests.Complete(session, ids[owner], (*slots)[index]));
	}
	requests.Reset(session + 1);
	for (const auto id : ids)
		CHECK_FALSE(requests.Claim(session, id));
}

TEST_CASE("Native registration results belong to one stack and execute once")
{
	NativeRegistrationRequests requests;
	CHECK(requests.Submit(0, 5) == -1);
	requests.Reset(1);
	const auto request = requests.Submit(1, 5);
	REQUIRE(request > 0);
	CHECK(requests.Take(1, 5, request) == NativeRegistrationRequests::kPending);
	CHECK(requests.Take(1, 6, request) == -1);
	requests.Cancel(1, 6, request);
	CHECK_FALSE(requests.Complete(1, request, 42));
	REQUIRE(requests.Claim(1, request));
	CHECK_FALSE(requests.Claim(1, request));
	REQUIRE(requests.Complete(1, request, 42));
	CHECK_FALSE(requests.Complete(1, request, 43));
	CHECK(requests.Take(1, 5, request) == 42);
	CHECK(requests.Take(1, 5, request) == -1);
}

TEST_CASE("Native registry requests cancel before dispatch and expire across saves")
{
	NativeRegistrationRequests requests;
	requests.Reset(1);
	const auto cancelled = requests.Submit(1, 5);
	requests.Cancel(1, 5, cancelled);
	CHECK_FALSE(requests.Claim(1, cancelled));
	const auto running = requests.Submit(1, 5);
	REQUIRE(requests.Claim(1, running));
	requests.Reset(2);
	CHECK_FALSE(requests.Complete(1, running, 10));
	CHECK(requests.Take(2, 5, running) == -1);
	const auto fresh = requests.Submit(2, 5);
	CHECK(fresh != running);
	CHECK_FALSE(requests.Claim(1, fresh));
	REQUIRE(requests.Claim(2, fresh));
	REQUIRE(requests.Complete(2, fresh, -2));
	CHECK(requests.Take(2, 5, fresh) == -2);
}

TEST_CASE("Native registry transport handles 2233 simultaneous callers without slot aliasing")
{
	NativeRegistrationRequests requests;
	requests.Reset(1);
	std::vector<std::int32_t> ids;
	for (std::uint32_t owner = 0; owner < 2233; ++owner) {
		const auto id = requests.Submit(1, owner);
		REQUIRE(id > 0);
		ids.push_back(id);
	}
	for (std::uint32_t owner = 0; owner < ids.size(); ++owner) {
		REQUIRE(requests.Claim(1, ids[owner]));
		REQUIRE(requests.Complete(1, ids[owner], static_cast<std::int32_t>(owner)));
		CHECK(requests.Take(1, owner + 1, ids[owner]) == -1);
		CHECK(requests.Take(1, owner, ids[owner]) == static_cast<std::int32_t>(owner));
	}
	CHECK(requests.Submit(1, 3000) > ids.back());
}
