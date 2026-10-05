#include "MCMBridge/Papyrus/HelperControlCapture.h"
#include "MCMBridge/Papyrus/HelperCustomCapture.h"
#include "MCMBridge/Papyrus/HelperMessageCapture.h"
#include "MCMBridge/Papyrus/HelperNativeUI.h"

#include <atomic>

namespace
{
	std::atomic_bool ready{};
}

namespace MCMBridge
{
	bool InstallHelperHostCapture()
	{
		const bool installed = InstallHelperMenuCapture() && InstallHelperControlCapture() &&
		                       InstallHelperCustomCapture() && InstallHelperMessageCapture();
		ready.store(installed);
		return installed;
	}

	bool IsHelperHostReady()
	{
		return ready.load();
	}
}
