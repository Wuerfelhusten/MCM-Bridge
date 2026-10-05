#include "MCMBridge/Plugin/StartupCompatibility.h"

#include <Windows.h>

namespace MCMBridge
{
	Result<bool> IsUnlockedSupportedAtStartup()
	{
		const bool present = GetModuleHandleW(L"MCM-Unlocked.dll") != nullptr;
		SKSE::log::info("Startup MCM Unlocked present={}; native host requires exclusive registry ownership", present);
		return !present;
	}
}
