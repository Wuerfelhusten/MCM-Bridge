#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

namespace MCMBridge
{
	enum class HelperMessageABI
	{
		kCoroutine,
		kCallback
	};
	enum class HelperHook : std::size_t
	{
		kMenu,
		kFlags,
		kNumber,
		kText,
		kValues,
		kCustom,
		kMessage
	};
	struct HelperFunctionFingerprint
	{
		std::size_t   offset{};
		std::size_t   size{};
		std::uint64_t hash{};
	};
	struct HelperBinaryProfile
	{
		std::string_view                         name;
		std::uint32_t                            timestamp;
		std::uint32_t                            imageSize;
		HelperMessageABI                         messageABI;
		std::array<HelperFunctionFingerprint, 7> hooks;
		std::array<HelperFunctionFingerprint, 2> consumers;
		const HelperFunctionFingerprint&         Function(HelperHook a_hook) const { return hooks[static_cast<std::size_t>(a_hook)]; }
	};
	std::span<const HelperBinaryProfile> HelperBinaryProfiles();
	const HelperBinaryProfile*           FindHelperBinaryProfile(std::uint32_t a_timestamp, std::uint32_t a_imageSize);
	bool                                 VerifyHelperBinaryProfile(const HelperBinaryProfile& a_profile, std::span<const std::byte> a_image);
}
