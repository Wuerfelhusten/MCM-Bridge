#include "MCMBridge/Core/NativeMCMRegistry.h"
#include "MCMBridge/Core/NativePreflightPolicy.h"

#include <catch2/catch_test_macros.hpp>

using namespace MCMBridge;

TEST_CASE("Native registry batches match sequential registration for 2233 menus")
{
	NativeMCMRegistry           batch;
	NativeMCMRegistry           sequential;
	const auto                  session = batch.BeginSession();
	const auto                  referenceSession = sequential.BeginSession();
	std::vector<NativeMCMEntry> entries;
	std::vector<std::int32_t>   expected;
	for (std::uint64_t index = 1; index <= 2233; ++index) {
		entries.push_back({ index, std::to_string(index), "Same display name" });
		auto result = sequential.Register(referenceSession, entries.back());
		REQUIRE(result);
		expected.push_back(*result);
	}
	const auto result = batch.RegisterBatch(session, entries);
	REQUIRE(result);
	CHECK(*result == expected);
	CHECK(batch.Count(session) == 2233);
	CHECK(batch.Revision() == sequential.Revision());
	for (const auto slot : expected) {
		REQUIRE(batch.Find(session, slot));
		CHECK(batch.Find(session, slot)->identity == sequential.Find(referenceSession, slot)->identity);
	}
	const auto revision = batch.Revision();
	const auto repeated = batch.RegisterBatch(session, entries);
	REQUIRE(repeated);
	CHECK(*repeated == *result);
	CHECK(batch.Revision() == revision);
}

TEST_CASE("Native registry batch conflicts leave earlier renames and additions unpublished")
{
	NativeMCMRegistry registry;
	const auto        session = registry.BeginSession();
	REQUIRE(registry.Register(session, { 1, "one", "Original" }) == 0);
	const auto revision = registry.Revision();
	for (const auto& invalid : std::vector<NativeMCMEntry>{ { 3, "one", "Duplicate identity" }, { 1, "different", "Changed identity" }, { 0, "zero", "Invalid" }, { 3, "", "Invalid" } }) {
		CHECK_FALSE(registry.RegisterBatch(session, { { 1, "one", "Renamed" }, { 2, "two", "New" }, invalid }));
		CHECK(registry.Revision() == revision);
		CHECK(registry.Count(session) == 1);
		CHECK(registry.Find(session, 0)->name == "Original");
		CHECK_FALSE(registry.FindSlot(session, 2));
	}
}

TEST_CASE("Native registry batches preserve sparse slots and ordered duplicate requests")
{
	NativeMCMRegistry registry;
	const auto        session = registry.BeginSession();
	REQUIRE(registry.Import(session, { std::nullopt, NativeMCMEntry{ 1, "one", "Original" }, std::nullopt }));
	const auto result = registry.RegisterBatch(session, { { 1, "one", "First" }, { 2, "two", "New" }, { 1, "one", "Last" } });
	REQUIRE(result);
	CHECK(*result == std::vector<std::int32_t>{ 1, 3, 1 });
	CHECK(registry.Find(session, 1)->name == "Last");
	CHECK_FALSE(registry.Find(session, 0));
	CHECK_FALSE(registry.Find(session, 2));
	REQUIRE(registry.Unregister(session, 1));
	REQUIRE(registry.RegisterBatch(session, { { 1, "one", "Again" } }));
	CHECK(registry.FindSlot(session, 1) == 4);
	const auto next = registry.BeginSession();
	CHECK_FALSE(registry.RegisterBatch(session, { { 3, "three", "Stale" } }));
	CHECK_FALSE(registry.RegisterBatch(session, {}));
	CHECK(registry.Count(next) == 0);
	CHECK(registry.RegisterBatch(next, {})->empty());
	CHECK(registry.Revision() == 1);
}

TEST_CASE("Native registry instance lookup survives sparse slots and rejects retired sessions")
{
	NativeMCMRegistry                          registry;
	const auto                                 session = registry.BeginSession();
	std::vector<std::optional<NativeMCMEntry>> slots(4097);
	for (std::size_t index = 0; index < 2000; ++index)
		slots[index * 2] = NativeMCMEntry{ index + 1, std::to_string(index), "Name" };
	REQUIRE(registry.Import(session, std::move(slots)));
	REQUIRE(registry.Count(session) == 2000);
	const auto revision = registry.Revision();
	for (std::uint64_t instance = 1; instance <= 2000; ++instance)
		CHECK(registry.FindSlot(session, instance) == static_cast<std::int32_t>((instance - 1) * 2));
	CHECK_FALSE(registry.FindSlot(session, 0));
	CHECK_FALSE(registry.FindSlot(session, 2001));
	CHECK(registry.Revision() == revision);
	REQUIRE(registry.Unregister(session, 128));
	CHECK(registry.Count(session) == 1999);
	CHECK_FALSE(registry.FindSlot(session, 128));
	CHECK(registry.FindSlot(session, 129) == 256);
	REQUIRE(registry.Register(session, { 128, "127", "Re-registered" }) == 4097);
	CHECK(registry.FindSlot(session, 128) == 4097);
	const auto next = registry.BeginSession();
	CHECK_FALSE(registry.Count(session));
	CHECK(registry.Count(next) == 0);
	CHECK_FALSE(registry.FindSlot(session, 128));
	CHECK_FALSE(registry.FindSlot(next, 128));
}

TEST_CASE("Native registry bootstrap preserves sparse saved slots without a menu limit")
{
	NativeMCMRegistry                          registry;
	const auto                                 session = registry.BeginSession();
	std::vector<std::optional<NativeMCMEntry>> slots(2401);
	for (std::size_t index = 0; index < 2000; ++index)
		slots[index + 400] = NativeMCMEntry{ index + 1, std::to_string(index), "Duplicate display name" };
	REQUIRE(registry.Import(session, slots));
	CHECK(registry.Read().count == 2000);
	CHECK(registry.Read().slots.size() == 2401);
	CHECK_FALSE(registry.Find(session, 399));
	REQUIRE(registry.Find(session, 400));
	CHECK(registry.Find(session, 400)->identity == "0");
	const auto existing = registry.Register(session, { 1, "0", "Renamed" });
	REQUIRE(existing);
	CHECK(*existing == 400);
	const auto next = registry.Register(session, { 2001, "new", "New" });
	REQUIRE(next);
	CHECK(*next == 2401);
	CHECK_FALSE(registry.Import(session, {}));
}

TEST_CASE("Invalid native registry imports never publish partial entries")
{
	NativeMCMRegistry registry;
	const auto        session = registry.BeginSession();
	const auto        before = registry.Read();
	CHECK_FALSE(registry.Import(session, { NativeMCMEntry{ 1, "same", "One" }, NativeMCMEntry{ 2, "same", "Two" } }));
	CHECK_FALSE(registry.Import(session, { NativeMCMEntry{ 1, "one", "One" }, NativeMCMEntry{ 1, "two", "Two" } }));
	CHECK_FALSE(registry.Import(session, { NativeMCMEntry{ 0, "one", "One" } }));
	CHECK(registry.Read().revision == before.revision);
	CHECK(registry.Read().count == 0);
	const auto current = registry.BeginSession();
	CHECK_FALSE(registry.Import(session, { NativeMCMEntry{ 1, "one", "One" } }));
	REQUIRE(registry.Import(current, { std::nullopt, NativeMCMEntry{ 1, "one", "One" } }));
	CHECK(registry.Read().count == 1);
	const auto emptySession = registry.BeginSession();
	REQUIRE(registry.Import(emptySession, {}));
	CHECK_FALSE(registry.Import(emptySession, {}));
}

TEST_CASE("Native mirror type admission ignores ASCII case but not different types")
{
	CHECK(EqualPapyrusTypeName("ski_configbase", "SKI_ConfigBase"));
	CHECK(EqualPapyrusTypeName("SkI_cOnFiGbAsE", "SKI_ConfigBase"));
	CHECK_FALSE(EqualPapyrusTypeName("SKI_ConfigBaseExtra", "SKI_ConfigBase"));
	CHECK_FALSE(EqualPapyrusTypeName("MCM_ConfigBase", "SKI_ConfigBase"));
	CHECK_FALSE(EqualPapyrusTypeName("", "SKI_ConfigBase"));
}

TEST_CASE("Native preflight retries changed registries without repeating identical notifications")
{
	NativePreflightGate gate;
	CHECK_FALSE(gate.ShouldRun(0, { "one" }));
	CHECK_FALSE(gate.ShouldRun(1, {}));
	CHECK(gate.ShouldRun(1, { "one" }));
	CHECK_FALSE(gate.ShouldRun(1, { "one" }));
	CHECK(gate.ShouldRun(1, { "one", "two" }));
	CHECK_FALSE(gate.ShouldRun(1, { "two", "one" }));
	CHECK(gate.ShouldRun(1, { "one", "three" }));
	CHECK(gate.ShouldRun(1, { "one" }));
	CHECK(gate.ShouldRun(2, { "one" }));
}

TEST_CASE("Native registry preserves all 2000 MCMs and compatibility slots")
{
	NativeMCMRegistry registry;
	const auto        session = registry.BeginSession();
	for (std::uint64_t index = 0; index < 2000; ++index) {
		const auto slot = registry.Register(session, { index + 1, std::to_string(index), "Same display name" });
		REQUIRE(slot);
		REQUIRE(*slot == static_cast<std::int32_t>(index));
	}
	const auto before = registry.Read();
	REQUIRE(before.count == 2000);
	for (const auto slot : { 0, 127, 128, 255, 256, 1279, 1280, 1999 }) {
		const auto entry = registry.Find(session, slot);
		REQUIRE(entry);
		CHECK(entry->identity == std::to_string(slot));
	}
	REQUIRE(registry.Unregister(session, 128));
	CHECK_FALSE(registry.Find(session, 127));
	CHECK(registry.Find(session, 128)->identity == "128");
	const auto replacement = registry.Register(session, { 3000, "new", "New" });
	REQUIRE(replacement);
	CHECK(*replacement == 2000);
	CHECK(before.slots[127]->identity == "127");
	CHECK(registry.Read().count == 2000);
}

TEST_CASE("Native registration is idempotent and does not equate names with identity")
{
	NativeMCMRegistry registry;
	const auto        session = registry.BeginSession();
	REQUIRE(registry.Register(session, { 1, "one", "Original" }));
	const auto revision = registry.Read().revision;
	CHECK(registry.Register(session, { 1, "one", "Original" }) == 0);
	CHECK(registry.Read().revision == revision);
	CHECK(registry.Register(session, { 1, "one", "Renamed" }) == 0);
	CHECK(registry.Read().revision == revision + 1);
	CHECK(registry.Find(session, 0)->identity == "one");
	CHECK_FALSE(registry.Register(session, { 2, "one", "Duplicate identity" }));
	CHECK_FALSE(registry.Register(session, { 1, "different", "Changed identity" }));
	CHECK(registry.Register(session, { 2, "two", "Renamed" }) == 1);
}

TEST_CASE("Native registry rejects old sessions and never revives removed slots")
{
	NativeMCMRegistry registry;
	CHECK_FALSE(registry.Register(0, { 1, "one", "One" }));
	const auto first = registry.BeginSession();
	REQUIRE(registry.Register(first, { 1, "one", "One" }));
	const auto second = registry.BeginSession();
	CHECK_FALSE(registry.Find(first, 0));
	CHECK_FALSE(registry.Register(first, { 1, "one", "One" }));
	CHECK_FALSE(registry.Unregister(first, 1));
	CHECK(registry.Read().count == 0);
	CHECK(registry.Register(second, { 1, "one", "One" }) == 0);
	CHECK_FALSE(registry.Register(second, { 0, "invalid", "" }));
	CHECK_FALSE(registry.Register(second, { 2, "", "" }));
	CHECK_FALSE(registry.Find(second, -1));
}
