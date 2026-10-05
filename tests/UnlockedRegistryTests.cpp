#include "MCMBridge/Core/BridgeSettings.h"
#include "MCMBridge/Core/UnlockedRegistryQuery.h"
#include <catch2/catch_test_macros.hpp>
#include <deque>

using namespace MCMBridge;

namespace
{
	struct RegistryFixture
	{
		UnlockedRegistryQuery::Clock::time_point now{};
		UnlockedRegistryQuery::Entries           entries{ { "Script::Original", "Alias", 100 } };
		std::deque<std::function<void()>>        tasks;
		std::function<void()>                    beforeVerification;
		std::optional<BridgeError>               entryError;
		int                                      counts{};
		std::shared_ptr<UnlockedRegistryQuery>   Create()
		{
			return std::make_shared<UnlockedRegistryQuery>([this](auto a_callback) { tasks.push_back([this, a_callback] {
																						 if (++counts == 2 && beforeVerification)
																							 beforeVerification();
																						 a_callback(static_cast<std::int32_t>(entries.size()));
																					 }); }, [this](std::int32_t a_index, auto a_callback) { tasks.push_back([this, a_index, a_callback] {
																																												   if (entryError)
																																													   a_callback(std::unexpected(*entryError));
																																												   else
																																													   a_callback(entries.at(static_cast<std::size_t>(a_index)));
																																											   }); }, [this] { return now; });
		}
		void Run()
		{
			std::size_t steps{};
			while (!tasks.empty()) {
				REQUIRE(++steps < 10000);
				REQUIRE(tasks.size() <= 8);
				auto task = std::move(tasks.front());
				tasks.pop_front();
				task();
			}
		}
	};
}

TEST_CASE("Unlocked reads authoritative IDs and aliases with no 128 entry limit")
{
	RegistryFixture fixture;
	for (std::uint64_t i = 1; i < 300; ++i) {
		fixture.entries.push_back({ "Script::" + std::to_string(i), "Same alias", 100 + i });
	}
	auto query = fixture.Create();
	query->Start();
	REQUIRE(query->Poll().error().code == BridgeErrorCode::kBusy);
	fixture.Run();
	const auto result = query->Poll();
	REQUIRE(result);
	REQUIRE(result->size() == 300);
	CHECK(result->front().id == "Script::Original");
	CHECK(result->front().displayName == "Alias");
	CHECK(fixture.counts == 3);
}

TEST_CASE("Unlocked does not publish a registry that changed during collection")
{
	RegistryFixture fixture;
	SECTION("count")
	{
		fixture.beforeVerification = [&] { fixture.entries.clear(); };
	}
	SECTION("identity")
	{
		fixture.beforeVerification = [&] { fixture.entries[0].id = "Replacement"; };
	}
	SECTION("marker")
	{
		fixture.beforeVerification = [&] { fixture.entries[0].marker = 999; };
	}
	auto query = fixture.Create();
	query->Start();
	fixture.Run();
	REQUIRE_FALSE(query->Poll());
	CHECK(query->Poll().error().code == BridgeErrorCode::kStaleSnapshot);
}

TEST_CASE("Unlocked rejects duplicate and missing registry entries")
{
	RegistryFixture fixture;
	SECTION("duplicate ID") { fixture.entries.push_back({ "Script::Original", "Other", 101 }); }
	SECTION("duplicate marker") { fixture.entries.push_back({ "Different", "Other", 100 }); }
	SECTION("missing ID") { fixture.entries[0].id.clear(); }
	SECTION("missing marker") { fixture.entries[0].marker = 0; }
	SECTION("dispatch failure") { fixture.entryError = BridgeError{ BridgeErrorCode::kDispatchFailed, "Unavailable" }; }
	auto query = fixture.Create();
	query->Start();
	fixture.Run();
	REQUIRE_FALSE(query->Poll());
	CHECK(query->Poll().error().code != BridgeErrorCode::kBusy);
}

TEST_CASE("Unlocked ignores late callbacks after timeout or save invalidation")
{
	RegistryFixture fixture;
	auto            query = fixture.Create();
	query->Start();
	BridgeErrorCode expected{};
	SECTION("timeout")
	{
		fixture.now += std::chrono::seconds(31);
		expected = BridgeErrorCode::kTimedOut;
	}
	SECTION("save invalidation")
	{
		query->Cancel();
		expected = BridgeErrorCode::kStaleSnapshot;
	}
	REQUIRE(query->Poll().error().code == expected);
	fixture.Run();
	CHECK(query->Poll().error().code == expected);
}

TEST_CASE("Unlocked permits an empty registry and synchronous adapters")
{
	auto query = std::make_shared<UnlockedRegistryQuery>([](auto a_callback) { a_callback(0); },
		[](std::int32_t, auto) { FAIL("Empty registry must not read entries"); },
		UnlockedRegistryQuery::Clock::now);
	query->Start();
	REQUIRE(query->Poll());
	CHECK(query->Poll()->empty());
}

TEST_CASE("Unlocked synchronous callbacks do not reenter the batch pump")
{
	auto query = std::make_shared<UnlockedRegistryQuery>([](auto a_callback) { a_callback(300); },
		[](std::int32_t a_index, auto a_callback) {
			a_callback(UnlockedRegistryEntry{ std::to_string(a_index), "Alias", static_cast<std::uint64_t>(a_index) + 1 });
		},
		UnlockedRegistryQuery::Clock::now);
	query->Start();
	REQUIRE(query->Poll());
	CHECK(query->Poll()->size() == 300);
}

TEST_CASE("Bridge aliases override provider aliases without changing original names")
{
	BridgeSettings    settings;
	const std::string original = "Original";
	settings.providerAliases["stable-id"] = "Unlocked alias";
	CHECK(ResolveMCMAlias(settings, "stable-id", original) == "Unlocked alias");
	SetMCMAlias(settings, "stable-id", "Bridge alias");
	CHECK(ResolveMCMAlias(settings, "stable-id", original) == "Bridge alias");
	SetMCMAlias(settings, "stable-id", "");
	CHECK(ResolveMCMAlias(settings, "stable-id", original) == "Unlocked alias");
	settings.providerAliases.clear();
	CHECK(ResolveMCMAlias(settings, "stable-id", original) == "Original");
	CHECK(original == "Original");
}
