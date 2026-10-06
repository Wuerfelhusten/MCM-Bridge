#pragma once

#include "MCMBridge/Core/BridgeSettings.h"
#include "MCMBridge/Core/Model.h"

namespace MCMBridge
{
	enum class Frontend
	{
		kNone,
		kMenuFramework,
		kFlick
	};

	constexpr Frontend SelectFrontend(bool a_flick, bool a_framework, bool a_preferFlick)
	{
		if (a_flick && (a_preferFlick || !a_framework))
			return Frontend::kFlick;
		return a_framework ? Frontend::kMenuFramework : Frontend::kNone;
	}

	std::string    FlickMCMGroup(const BridgeSettings& a_settings, std::string_view a_name);
	std::string    FlickPageLabel(const MCMPage& a_page);
	constexpr bool FlickSettingsVisible(Frontend a_active, bool a_preferFlick)
	{
		return a_active == Frontend::kFlick && a_preferFlick;
	}
}
