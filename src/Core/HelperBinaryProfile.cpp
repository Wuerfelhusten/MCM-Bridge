#include "MCMBridge/Core/HelperBinaryProfile.h"
#include "MCMBridge/Core/HelperMenuCapture.h"

namespace MCMBridge
{
	std::span<const HelperBinaryProfile> HelperBinaryProfiles()
	{
		// Release DLLs with matching PDBs. Ranges contain no PE base relocations.
		static constexpr std::array profiles{
			HelperBinaryProfile{ "1.4.0 SE backport", 0x63404841, 0xD3000, HelperMessageABI::kCoroutine,
				{ { { 0x64AF0, 951, 0x92848FB4291116ADULL }, { 0x65550, 917, 0x49E7E1A3DDC7BC81ULL },
					{ 0x658F0, 721, 0x68ECFEA338E51039ULL }, { 0x65BD0, 722, 0xF3CBACD18AAFFE93ULL },
					{ 0x65EB0, 785, 0xCD0C6A334AC8F5E1ULL }, { 0x19E10, 69, 0xDCD436D7EBA5CCBEULL },
					{ 0x68540, 127, 0x79A5DDF40DEE305EULL } } },
				{ { { 0x5F7A0, 55, 0x089C551C27D8E7A5ULL }, { 0x5F7E0, 46, 0x04E1770F7FFF66A0ULL } } } },
			HelperBinaryProfile{ "1.4.0 AE", 0x63404575, 0xD4000, HelperMessageABI::kCoroutine,
				{ { { 0x64E80, 951, 0x8DB540B1607822BAULL }, { 0x658E0, 917, 0x1C6AF17F7EE60D32ULL },
					{ 0x65C80, 721, 0xB3F6AC174ED10392ULL }, { 0x65F60, 722, 0x7938CD45EBE0E6CCULL },
					{ 0x66240, 785, 0x8385D7B22A8D1044ULL }, { 0x1A1A0, 69, 0xDCD436D7EBA5CCBEULL },
					{ 0x688E0, 127, 0xF780FD9E84C0BE07ULL } } },
				{ { { 0x5FB30, 55, 0x5ECD6E019B73F1D0ULL }, { 0x5FB70, 46, 0x04E1770F7FFF66A0ULL } } } },
			HelperBinaryProfile{ "1.5.0 SE/AE", 0x657F508D, 0xDD000, HelperMessageABI::kCoroutine,
				{ { { 0x57A90, 956, 0xA5A875A7FB6A2FD9ULL }, { 0x584F0, 917, 0xBBB97E9F91A72FAEULL },
					{ 0x58890, 721, 0xD9332B6E94910A28ULL }, { 0x58B70, 722, 0x370715D708D8DDBBULL },
					{ 0x58E50, 785, 0x2C4F20F4DB22A73CULL }, { 0x1A810, 69, 0xD3BEF0973F2686A8ULL },
					{ 0x5B510, 127, 0x5886AD1CFCCF3CFEULL } } },
				{ { { 0x52370, 55, 0x971EC550464629AAULL }, { 0x523B0, 46, 0x04E1770F7FFF66A0ULL } } } },
			HelperBinaryProfile{ "1.6.2 SE/AE", 0x69EE35AA, 0x11F000, HelperMessageABI::kCallback,
				{ { { 0x5A630, 913, 0x72C7362320BBD5FEULL }, { 0x5AF90, 843, 0x1E493D6186A33538ULL },
					{ 0x5B2E0, 709, 0x9BA01348F393BC38ULL }, { 0x5B5B0, 710, 0xF0495CFBF9555712ULL },
					{ 0x5B880, 773, 0xB0634E60F45413A2ULL }, { 0x234E0, 58, 0x0F121ED55482B8B5ULL },
					{ 0x5CFE0, 1927, 0x1E807A441FEAE712ULL } } },
				{} },
			HelperBinaryProfile{ "1.6.3 SE/AE", 0x6A8F2A7C, 0x122000, HelperMessageABI::kCallback,
				{ { { 0x5A590, 913, 0xFD790D890BE6C8C4ULL }, { 0x5AEF0, 843, 0x487E875577A714DCULL },
					{ 0x5B240, 709, 0x3CD596AF8BAD1AEEULL }, { 0x5B510, 710, 0x72402874E37CEFD4ULL },
					{ 0x5B7E0, 773, 0x32A469AB0CABF171ULL }, { 0x235C0, 58, 0x725FD5E7C2157B8CULL },
					{ 0x5CF40, 1839, 0x57C0453AAA58F534ULL } } },
				{} }
		};
		return profiles;
	}
	const HelperBinaryProfile* FindHelperBinaryProfile(std::uint32_t a_timestamp, std::uint32_t a_imageSize)
	{
		for (const auto& profile : HelperBinaryProfiles())
			if (profile.timestamp == a_timestamp && profile.imageSize == a_imageSize)
				return &profile;
		return nullptr;
	}
	bool VerifyHelperBinaryProfile(const HelperBinaryProfile& a_profile, std::span<const std::byte> a_image)
	{
		if (a_image.size() != a_profile.imageSize)
			return false;
		const auto valid = [&](const HelperFunctionFingerprint& a_function) {
			return a_function.size && a_function.offset <= a_image.size() && a_function.size <= a_image.size() - a_function.offset &&
			       HelperCodeFingerprint(a_image.subspan(a_function.offset, a_function.size)) == a_function.hash;
		};
		for (const auto& function : a_profile.hooks)
			if (!valid(function))
				return false;
		for (const auto& function : a_profile.consumers)
			if (function.size && !valid(function))
				return false;
		return a_profile.messageABI != HelperMessageABI::kCoroutine || (a_profile.consumers[0].size && a_profile.consumers[1].size);
	}
}
