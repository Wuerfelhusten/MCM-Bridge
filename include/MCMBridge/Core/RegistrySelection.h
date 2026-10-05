#pragma once

namespace MCMBridge
{
	enum class RegistrySource
	{
		kClassicSkyUI,
		kMCMUnlocked
	};

	constexpr RegistrySource SelectRegistrySource(bool a_mcmUnlockedAvailable)
	{
		return a_mcmUnlockedAvailable ? RegistrySource::kMCMUnlocked : RegistrySource::kClassicSkyUI;
	}
}
