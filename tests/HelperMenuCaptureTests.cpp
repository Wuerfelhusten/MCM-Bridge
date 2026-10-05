#include "MCMBridge/Core/HelperCustomContent.h"
#include "MCMBridge/Core/HelperMenuCapture.h"
#include "MCMBridge/Core/NativeHostSession.h"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <limits>

using namespace MCMBridge;

TEST_CASE("Native menu indices accept integral Helper floats without unsafe conversion")
{
	CHECK(ConvertMenuDialogIndex(-1.0) == -1);
	CHECK(ConvertMenuDialogIndex(0.0) == 0);
	CHECK(ConvertMenuDialogIndex(5000.0) == 5000);
	CHECK(ConvertMenuDialogIndex(2147483647.0) == 2147483647);
	CHECK_FALSE(ConvertMenuDialogIndex(2147483648.0));
	CHECK_FALSE(ConvertMenuDialogIndex(-2.0));
	CHECK_FALSE(ConvertMenuDialogIndex(1.5));
	CHECK_FALSE(ConvertMenuDialogIndex(std::numeric_limits<double>::infinity()));
	CHECK_FALSE(ConvertMenuDialogIndex(std::numeric_limits<double>::quiet_NaN()));
}

namespace
{
	void Word(std::span<std::byte> a_bytes, std::size_t a_offset, std::uint64_t a_value)
	{
		std::memcpy(a_bytes.data() + a_offset, &a_value, sizeof(a_value));
	}
}

TEST_CASE("Helper release string records copy complete menus independently of host STL layout")
{
	std::array<std::byte, 96> records{};
	const std::string         shortText = "$One";
	std::string               longText = "A menu option longer than inline storage";
	std::memcpy(records.data(), shortText.data(), shortText.size());
	Word(records, 16, shortText.size());
	Word(records, 24, 15);
	Word(records, 32, reinterpret_cast<std::uintptr_t>(longText.data()));
	Word(records, 48, longText.size());
	Word(records, 56, longText.size());
	Word(records, 88, 15);
	auto copied = CopyHelperMenuStrings(records);
	REQUIRE(copied);
	REQUIRE(copied->size() == 3);
	CHECK((*copied)[0] == shortText);
	CHECK((*copied)[1] == longText);
	CHECK((*copied)[2].empty());
	const auto saved = *copied;
	longText.assign("Changed source");
	records.fill(std::byte{});
	CHECK(*copied == saved);
	CHECK(CopyHelperMenuStrings({})->empty());
}

TEST_CASE("Helper string capture rejects invalid layouts without partial menus")
{
	std::array<std::byte, 64> records{};
	Word(records, 24, 15);
	CHECK_FALSE(CopyHelperMenuStrings(std::span(records).first(63)));
	Word(records, 48, 16);
	Word(records, 56, 15);
	CHECK_FALSE(CopyHelperMenuStrings(records));
	Word(records, 56, 16);
	CHECK_FALSE(CopyHelperMenuStrings(records));
	Word(records, 48, 0x80000000ULL);
	Word(records, 56, 0x80000000ULL);
	CHECK_FALSE(CopyHelperMenuStrings(records));
}

TEST_CASE("Helper native options require the current execution owner and dialog request")
{
	NativeHostSession host;
	host.Reset(1);
	const auto token = host.Open(1, "Helper MCM", 99);
	REQUIRE(token);
	CHECK(host.TokenForOwner(99) == *token);
	CHECK(host.TokenForOwner(100) == 0);
	CHECK(host.TokenForOwner(0) == 0);
	REQUIRE(host.BeginPage(*token, "Page", 0));
	const auto option = host.AddOption(*token, 5, "Menu", "One", 0, 0, "");
	REQUIRE(host.Publish(*token));
	const auto request = host.BeginDialog(*token, option);
	REQUIRE(request > 0);
	CHECK(host.ActiveDialogRequest(*token, 5) == request);
	CHECK(host.ActiveDialogRequest(*token, 4) == 0);
	CHECK(host.ActiveDialogRequest(*token + 1, 5) == 0);
	REQUIRE(host.SetDialogOptions(request, { "One", "Two", "Three" }));
	REQUIRE(host.FinishDialog(*token, request));
	CHECK(host.ReadDialog(*token)->options.size() == 3);
	CHECK(host.ActiveDialogRequest(*token, 5) == 0);
	REQUIRE(host.Close(*token));
	CHECK(host.TokenForOwner(99) == 0);
	CHECK_FALSE(host.SetDialogOptions(request, { "Late" }));
	const auto next = host.Open(1, "Helper MCM", 99);
	REQUIRE(next);
	CHECK(host.TokenForOwner(99) == *next);
	CHECK(*next != *token);
	host.Reset(2);
	CHECK(host.TokenForOwner(99) == 0);
}

TEST_CASE("Helper code fingerprints detect changed instructions")
{
	const std::array bytes{ std::byte{ 'a' }, std::byte{ 'b' }, std::byte{ 'c' } };
	CHECK(HelperCodeFingerprint(bytes) == 0xE71FA2190541574BULL);
	auto changed = bytes;
	changed[1] ^= std::byte{ 1 };
	CHECK(HelperCodeFingerprint(changed) != HelperCodeFingerprint(bytes));
}

TEST_CASE("Helper custom content copies source and offsets without borrowing release objects")
{
	std::array<std::byte, 48> record{};
	std::string               source = "Interface/Custom/Example.dds";
	Word(record, 8, reinterpret_cast<std::uintptr_t>(source.data()));
	Word(record, 24, source.size());
	Word(record, 32, source.size());
	const float x = 22.5F;
	const float y = -9.0F;
	std::memcpy(record.data() + 40, &x, sizeof(x));
	std::memcpy(record.data() + 44, &y, sizeof(y));
	auto content = CopyHelperCustomContent(record);
	REQUIRE(content);
	CHECK(content->source == source);
	CHECK(content->x == x);
	CHECK(content->y == y);
	source.assign("changed");
	CHECK(content->source == "Interface/Custom/Example.dds");
	record.fill(std::byte{});
	std::memcpy(record.data() + 8, "page.swf", 8);
	Word(record, 24, 8);
	Word(record, 32, 15);
	content = CopyHelperCustomContent(record);
	REQUIRE(content);
	CHECK(content->source == "page.swf");
	CHECK(content->x == 0);
	CHECK(content->y == 0);
	NativeHostSession host;
	host.Reset(1);
	const auto token = host.Open(1, "mod", 99);
	REQUIRE(token);
	REQUIRE(host.BeginPage(*token, "Custom", 0));
	REQUIRE(host.SetCustomContent(*token, content->source, content->x, content->y));
	CHECK_FALSE(host.Read());
	REQUIRE(host.Publish(*token));
	CHECK(host.Read()->customSource == "page.swf");
	REQUIRE(host.BeginPage(*token, "Controls", 1));
	REQUIRE(host.Publish(*token));
	CHECK(host.Read()->customSource.empty());
	REQUIRE(host.Close(*token));
	CHECK_FALSE(host.SetCustomContent(*token, "late.dds", x, y));
}

TEST_CASE("Helper custom content rejects malformed layouts and nonfinite coordinates")
{
	std::array<std::byte, 48> record{};
	Word(record, 32, 15);
	CHECK_FALSE(CopyHelperCustomContent(std::span(record).first(47)));
	Word(record, 24, 16);
	CHECK_FALSE(CopyHelperCustomContent(record));
	Word(record, 24, 0);
	const float invalid = std::numeric_limits<float>::infinity();
	std::memcpy(record.data() + 40, &invalid, sizeof(invalid));
	CHECK_FALSE(CopyHelperCustomContent(record));
	record.fill(std::byte{});
	Word(record, 32, 15);
	Word(record, 24, 1);
	CHECK_FALSE(CopyHelperCustomContent(record));
}
