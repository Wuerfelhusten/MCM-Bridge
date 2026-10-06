#include "MCMBridge/Core/BridgeSettings.h"
#include "MCMBridge/Core/MCMOrganization.h"

#include <SimpleIni.h>
#include <algorithm>

namespace
{
	MCMBridge::Result<void> Load(CSimpleIniA& a_ini, const std::filesystem::path& a_path)
	{
		std::error_code error;
		const auto      exists = std::filesystem::exists(a_path, error);
		if (error || (exists && a_ini.LoadFile(a_path.string().c_str()) < 0)) {
			return std::unexpected(MCMBridge::BridgeError{
				MCMBridge::BridgeErrorCode::kIoError, "Could not read MCMBridge.ini" });
		}
		return {};
	}
}

namespace MCMBridge
{
	std::string ResolveMCMAlias(const BridgeSettings& a_settings, std::string_view a_modID, std::string_view a_original)
	{
		const auto found = a_settings.aliases.find(a_modID);
		if (found != a_settings.aliases.end())
			return found->second;
		const auto provider = a_settings.providerAliases.find(a_modID);
		return provider != a_settings.providerAliases.end() ? provider->second : std::string(a_original);
	}

	void SetMCMAlias(BridgeSettings& a_settings, std::string a_modID, std::string a_alias)
	{
		if (a_modID.empty())
			return;
		for (auto& character : a_alias) {
			if (static_cast<unsigned char>(character) < 32 || character == 127 || character == '#')
				character = ' ';
		}
		const auto first = a_alias.find_first_not_of(' ');
		if (first == std::string::npos) {
			a_settings.aliases.erase(a_modID);
			return;
		}
		a_alias = a_alias.substr(first, a_alias.find_last_not_of(' ') - first + 1);
		a_settings.aliases.insert_or_assign(std::move(a_modID), std::move(a_alias));
	}

	Result<BridgeSettings> LoadBridgeSettings(const std::filesystem::path& a_path)
	{
		CSimpleIniA ini;
		ini.SetUnicode();
		if (auto result = Load(ini, a_path); !result)
			return std::unexpected(result.error());
		BridgeSettings settings{
			.pauseDuringWrites = ini.GetBoolValue("General", "PauseDuringWrites", true),
			.closeJournalOnRedirect = ini.GetBoolValue("General", "CloseJournalOnRedirect", true),
			.preferFlick = ini.GetBoolValue("General", "PreferFLICK", true),
			.groupMCMs = ini.GetBoolValue("General", "GroupMCMs", false),
			.alphabeticMCMs = ini.GetBoolValue("General", "AlphabeticMCMs", false),
			.mcmRangeEnds = ini.GetValue("General", "MCMRangeEnds", "CGLRZ")
		};
		if (!ValidMCMRanges(settings.mcmRangeEnds))
			settings.mcmRangeEnds = "CGLRZ";
		const auto selector = ini.GetLongValue("FLICK", "PageSelector", 1);
		settings.flickPageSelector = selector >= 0 && selector <= 2 ? static_cast<FlickPageSelector>(selector) : FlickPageSelector::kLeft;
		settings.flickPageRows = static_cast<int>(std::clamp(ini.GetLongValue("FLICK", "PageRows", 3), 1L, 6L));
		settings.flickPageWidth = static_cast<int>(std::clamp(ini.GetLongValue("FLICK", "PageWidth", 25), 15L, 45L));
		CSimpleIniA::TNamesDepend keys;
		ini.GetAllKeys("Aliases", keys);
		for (const auto& key : keys) {
			SetMCMAlias(settings, key.pItem, ini.GetValue("Aliases", key.pItem, ""));
		}
		return settings;
	}

	Result<void> SaveBridgeSettings(const std::filesystem::path& a_path, const BridgeSettings& a_settings)
	{
		CSimpleIniA ini;
		ini.SetUnicode();
		if (auto result = Load(ini, a_path); !result)
			return result;
		ini.Delete("Aliases", nullptr);
		ini.Delete("General", "RedirectMCM");
		for (const auto& [modID, alias] : a_settings.aliases) {
			if (ini.SetValue("Aliases", modID.c_str(), alias.c_str()) < 0) {
				return std::unexpected(BridgeError{ BridgeErrorCode::kIoError, "Could not save MCM aliases" });
			}
		}
		std::error_code error;
		if (!a_path.parent_path().empty())
			std::filesystem::create_directories(a_path.parent_path(), error);
		if (error || ini.SetBoolValue("General", "PauseDuringWrites", a_settings.pauseDuringWrites) < 0 ||
			ini.SetBoolValue("General", "CloseJournalOnRedirect", a_settings.closeJournalOnRedirect) < 0 ||
			ini.SetBoolValue("General", "PreferFLICK", a_settings.preferFlick) < 0 ||
			ini.SetBoolValue("General", "GroupMCMs", a_settings.groupMCMs) < 0 ||
			ini.SetBoolValue("General", "AlphabeticMCMs", a_settings.alphabeticMCMs) < 0 ||
			ini.SetValue("General", "MCMRangeEnds", a_settings.mcmRangeEnds.c_str()) < 0 ||
			ini.SetLongValue("FLICK", "PageSelector", static_cast<long>(a_settings.flickPageSelector)) < 0 ||
			ini.SetLongValue("FLICK", "PageRows", a_settings.flickPageRows) < 0 ||
			ini.SetLongValue("FLICK", "PageWidth", a_settings.flickPageWidth) < 0 ||
			ini.SaveFile(a_path.string().c_str()) < 0) {
			return std::unexpected(BridgeError{ BridgeErrorCode::kIoError, "Could not save MCMBridge.ini" });
		}
		return {};
	}
}
