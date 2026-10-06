#include "MCMBridge/Core/HelperBinaryProfile.h"
#include "MCMBridge/Core/HelperMenuCapture.h"
#include <catch2/catch_test_macros.hpp>
#include <limits>

using namespace MCMBridge;

TEST_CASE("Helper release profiles select an exact binary and its message ABI", "[helper][admission]")
{
	const auto profiles = HelperBinaryProfiles();
	REQUIRE(profiles.size() == 6);
	for (const auto& profile : profiles) {
		REQUIRE(FindHelperBinaryProfile(profile.timestamp, profile.imageSize) == &profile);
		REQUIRE_FALSE(FindHelperBinaryProfile(profile.timestamp + 1, profile.imageSize));
		REQUIRE_FALSE(FindHelperBinaryProfile(profile.timestamp, profile.imageSize + 1));
		for (const auto& function : profile.hooks) {
			REQUIRE(function.size > 0);
			REQUIRE(function.offset + function.size < profile.imageSize);
		}
		if (profile.messageABI == HelperMessageABI::kCoroutine) {
			REQUIRE(profile.consumers[0].size > 0);
			REQUIRE(profile.consumers[1].size > 0);
		}
	}
	REQUIRE_FALSE(FindHelperBinaryProfile(0x612693CE, 0x116000));
	REQUIRE_FALSE(FindHelperBinaryProfile(0x619B57BE, 0x121000));
}

TEST_CASE("Helper 1.6.3 SE backport selects its own hooks and callback message ABI", "[helper][admission]")
{
	const auto* profile = FindHelperBinaryProfile(0x6AAF26F9, 0x155000);
	REQUIRE(profile);
	REQUIRE(profile->name == "1.6.3 SE 1.5.97 backport");
	REQUIRE(profile->messageABI == HelperMessageABI::kCallback);
	REQUIRE(profile->Function(HelperHook::kMenu).offset == 0x68DC0);
	REQUIRE(profile->Function(HelperHook::kCustom).offset == 0x30100);
	REQUIRE(profile->Function(HelperHook::kMessage).offset == 0x6BAB0);
	REQUIRE(profile->Function(HelperHook::kMessage).size == 2123);
	REQUIRE_FALSE(FindHelperBinaryProfile(0x6AAF26F9, 0x122000));
	REQUIRE_FALSE(FindHelperBinaryProfile(0x6A8F2A7C, 0x155000));
	const auto* original = FindHelperBinaryProfile(0x6A8F2A7C, 0x122000);
	REQUIRE(original);
	REQUIRE(original != profile);
	REQUIRE(original->Function(HelperHook::kMessage).offset == 0x5CF40);
}

TEST_CASE("Helper admission validates every hook before allowing any patch", "[helper][admission]")
{
	std::array<std::byte, 16> image{};
	HelperBinaryProfile       profile{ "fixture", 1, 16, HelperMessageABI::kCallback, {}, {} };
	const auto                hash = HelperCodeFingerprint(std::span(image).first(1));
	for (std::size_t index = 0; index < profile.hooks.size(); ++index)
		profile.hooks[index] = { index, 1, hash };
	REQUIRE(VerifyHelperBinaryProfile(profile, image));
	for (std::size_t index = 0; index < profile.hooks.size(); ++index) {
		image[index] = std::byte{ 1 };
		REQUIRE_FALSE(VerifyHelperBinaryProfile(profile, image));
		image[index] = std::byte{};
	}
	REQUIRE_FALSE(VerifyHelperBinaryProfile(profile, std::span(image).first(15)));
	profile.hooks[0].offset = std::numeric_limits<std::size_t>::max();
	REQUIRE_FALSE(VerifyHelperBinaryProfile(profile, image));
}

TEST_CASE("Helper coroutine admission requires both verified consumer functions", "[helper][admission]")
{
	std::array<std::byte, 16> image{};
	HelperBinaryProfile       profile{ "fixture", 1, 16, HelperMessageABI::kCoroutine, {}, {} };
	const auto                hash = HelperCodeFingerprint(std::span(image).first(1));
	for (auto& function : profile.hooks) function = { 0, 1, hash };
	REQUIRE_FALSE(VerifyHelperBinaryProfile(profile, image));
	profile.consumers = { { { 8, 1, hash }, { 9, 1, hash } } };
	REQUIRE(VerifyHelperBinaryProfile(profile, image));
	image[9] = std::byte{ 1 };
	REQUIRE_FALSE(VerifyHelperBinaryProfile(profile, image));
}
