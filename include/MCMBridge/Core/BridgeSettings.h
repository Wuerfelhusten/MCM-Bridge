#pragma once

#include "MCMBridge/Core/Result.h"

#include <filesystem>
#include <map>
#include <string>
#include <string_view>

namespace MCMBridge
{
	enum class FlickPageSelector
	{
		kTop,
		kLeft,
		kDropdown
	};

	struct BridgeSettings
	{
		bool                                            pauseDuringWrites{ true };
		bool                                            closeJournalOnRedirect{ true };
		bool                                            preferFlick{ true };
		bool                                            groupMCMs{};
		bool                                            alphabeticMCMs{};
		std::string                                     mcmRangeEnds{ "CGLRZ" };
		std::map<std::string, std::string, std::less<>> aliases;
		// Session-only provider aliases are never written to MCMBridge.ini.
		std::map<std::string, std::string, std::less<>> providerAliases;
		FlickPageSelector                               flickPageSelector{ FlickPageSelector::kLeft };
		int                                             flickPageRows{ 3 };
		int                                             flickPageWidth{ 25 };
	};

	// Aliases are presentation preferences, never MCM identities or snapshot data.
	std::string ResolveMCMAlias(const BridgeSettings& a_settings, std::string_view a_modID, std::string_view a_original);
	void        SetMCMAlias(BridgeSettings& a_settings, std::string a_modID, std::string a_alias);

	Result<BridgeSettings> LoadBridgeSettings(const std::filesystem::path& a_path);
	Result<void>           SaveBridgeSettings(const std::filesystem::path& a_path, const BridgeSettings& a_settings);
}
