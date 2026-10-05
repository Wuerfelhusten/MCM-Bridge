#pragma once

#include "MCMBridge/Core/BridgeSettings.h"

#include <mutex>

namespace MCMBridge
{
	class BridgeSettingsService
	{
	public:
		static BridgeSettingsService& GetSingleton();
		void                          Load();
		BridgeSettings                Get() const;
		void                          SetPauseDuringWrites(bool a_value);
		void                          SetCloseJournalOnRedirect(bool a_value);
		void                          SetGroupMCMs(bool a_value);
		void                          SetMCMRanges(bool a_enabled, std::string a_ends);
		void                          SetAlias(std::string a_modID, std::string a_alias);
		void                          SetProviderAliases(std::map<std::string, std::string, std::less<>> a_aliases);

	private:
		void               Set(bool BridgeSettings::* a_member, bool a_value);
		void               Save() const;
		mutable std::mutex mutex;
		BridgeSettings     settings;
	};
}
