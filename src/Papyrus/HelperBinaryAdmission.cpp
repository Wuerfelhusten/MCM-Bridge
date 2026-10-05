#include "MCMBridge/Papyrus/HelperBinaryAdmission.h"

namespace MCMBridge
{
	const HelperBinaryProfile* LoadedHelperBinaryProfile()
	{
		// Called at PostLoad before any capture hook changes the verified bytes.
		static const auto* admitted = []() -> const HelperBinaryProfile* {
			const auto module = GetModuleHandleW(L"MCMHelper.dll");
			if (!module)
				return nullptr;
			const auto* base = reinterpret_cast<const std::byte*>(module);
			const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
			if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 || dos->e_lfanew > 4096)
				return nullptr;
			const auto* pe = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
			if (pe->Signature != IMAGE_NT_SIGNATURE || pe->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
				pe->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
				return nullptr;
			const auto* profile = FindHelperBinaryProfile(pe->FileHeader.TimeDateStamp, pe->OptionalHeader.SizeOfImage);
			if (!profile || !VerifyHelperBinaryProfile(*profile, { base, profile->imageSize })) {
				SKSE::log::error("Helper capture rejected: unknown or modified binary timestamp={:X} image_size={:X}; no code patched",
					pe->FileHeader.TimeDateStamp, pe->OptionalHeader.SizeOfImage);
				return nullptr;
			}
			SKSE::log::info("Helper capture profile admitted: {}", profile->name);
			return profile;
		}();
		return admitted;
	}
}
