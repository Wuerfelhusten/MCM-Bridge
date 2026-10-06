#include "MCMBridge/Core/FrontendSelection.h"
#include "MCMBridge/Core/MCMOrganization.h"
#include "MCMBridge/Core/SkyUIRichText.h"

namespace MCMBridge
{
	std::string FlickMCMGroup(const BridgeSettings& a_settings, std::string_view a_name)
	{
		if (!a_settings.groupMCMs)
			return {};
		return a_settings.alphabeticMCMs ? "MCM - " + MCMNameRange(a_name, a_settings.mcmRangeEnds) : "MCMs";
	}

	std::string FlickPageLabel(const MCMPage& a_page)
	{
		// An unnamed default page is valid MCM data. Give it a visible label
		// without changing the raw name or its navigation/Memory identity.
		auto label = PlainSkyUIText(a_page.displayName.empty() ? a_page.rawName : a_page.displayName);
		std::ranges::replace(label, '#', ' ');
		return label.find_first_not_of(" \t\r\n") == std::string::npos ? "General" : label;
	}
}
