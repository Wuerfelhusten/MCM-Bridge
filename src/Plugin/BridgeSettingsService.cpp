#include "MCMBridge/Plugin/BridgeSettingsService.h"
#include "MCMBridge/Core/MCMOrganization.h"

#include "MCMBridge/Framework/FrameworkApi.h"
#include "MCMBridge/Plugin/BridgeController.h"
#include "MCMBridge/UI/WriteNotifications.h"

namespace
{
	const std::filesystem::path settingsPath = "Data/SKSE/plugins/MCMBridge.ini";
}

namespace MCMBridge
{
	BridgeSettingsService& BridgeSettingsService::GetSingleton()
	{
		static BridgeSettingsService service;
		return service;
	}

	void BridgeSettingsService::Load()
	{
		const auto             result = LoadBridgeSettings(settingsPath);
		const std::scoped_lock lock(mutex);
		settings = result ? *result : BridgeSettings{};
		if (!result)
			SKSE::log::warn("{}; using default settings", result.error().message);
	}

	BridgeSettings BridgeSettingsService::Get() const
	{
		const std::scoped_lock lock(mutex);
		return settings;
	}

	void BridgeSettingsService::SetPauseDuringWrites(bool a_value)
	{
		Set(&BridgeSettings::pauseDuringWrites, a_value);
	}

	void BridgeSettingsService::SetCloseJournalOnRedirect(bool a_value)
	{
		Set(&BridgeSettings::closeJournalOnRedirect, a_value);
	}

	void BridgeSettingsService::SetGroupMCMs(bool a_value)
	{
		Set(&BridgeSettings::groupMCMs, a_value);
		if (auto* tasks = SKSE::GetTaskInterface()) {
			tasks->AddTask([] {
				const auto snapshot = BridgeController::GetSingleton().Snapshot();
				FrameworkApi::GetSingleton().SynchronizeMCMs(snapshot->mods);
			});
		}
	}

	void BridgeSettingsService::SetMCMRanges(bool a_enabled, std::string a_ends)
	{
		if (!ValidMCMRanges(a_ends))
			return;
		{
			const std::scoped_lock lock(mutex);
			settings.alphabeticMCMs = a_enabled;
			settings.mcmRangeEnds = std::move(a_ends);
		}
		if (auto* tasks = SKSE::GetTaskInterface()) {
			tasks->AddTask([] {
				GetSingleton().Save();
				FrameworkApi::GetSingleton().SynchronizeMCMs(BridgeController::GetSingleton().Snapshot()->mods);
			});
		}
	}

	void BridgeSettingsService::Set(bool BridgeSettings::* a_member, bool a_value)
	{
		{
			const std::scoped_lock lock(mutex);
			settings.*a_member = a_value;
		}
		if (auto* tasks = SKSE::GetTaskInterface()) {
			tasks->AddTask([] { GetSingleton().Save(); });
		}
	}

	void BridgeSettingsService::Save() const
	{
		const std::scoped_lock lock(mutex);
		if (const auto result = SaveBridgeSettings(settingsPath, settings); !result) {
			SKSE::log::error("{}", result.error().message);
			WriteNotifications::Show(result.error().message);
		}
	}

	void BridgeSettingsService::SetAlias(std::string a_modID, std::string a_alias)
	{
		{
			const std::scoped_lock lock(mutex);
			SetMCMAlias(settings, std::move(a_modID), std::move(a_alias));
		}
		if (auto* tasks = SKSE::GetTaskInterface()) {
			tasks->AddTask([] {
				GetSingleton().Save();
				const auto snapshot = BridgeController::GetSingleton().Snapshot();
				FrameworkApi::GetSingleton().SynchronizeMCMs(snapshot->mods);
			});
		}
	}

	void BridgeSettingsService::SetProviderAliases(std::map<std::string, std::string, std::less<>> a_aliases)
	{
		BridgeSettings normalized;
		for (auto& [id, alias] : a_aliases) SetMCMAlias(normalized, id, std::move(alias));
		const std::scoped_lock lock(mutex);
		settings.providerAliases = std::move(normalized.aliases);
	}
}
