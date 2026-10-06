#include "MCMBridge/Framework/FrameworkApi.h"

#include "MCMBridge/Framework/FrameworkRequirements.h"
#include "MCMBridge/Framework/MCMEntryRegistry.h"
#include "MCMBridge/Plugin/BridgeController.h"
#include "MCMBridge/UI/BrowserWindow.h"
#include "MCMBridge/UI/KeybindSelector.h"
#include "MCMBridge/UI/MessageDialog.h"
#include "MCMBridge/UI/QuickOpenWindow.h"
#include "MCMBridge/UI/SettingsWindow.h"
#include "MCMBridge/UI/WriteNotifications.h"
#include "SKSEMenuFramework.h"

namespace
{
	std::unique_ptr<SKSEMenuFramework::Model::Event> lifecycleRegistration;

	void __stdcall OnFrameworkEvent(SKSEMenuFramework::Model::EventType a_eventType)
	{
		auto& controller = MCMBridge::BridgeController::GetSingleton();
		switch (a_eventType) {
		case SKSEMenuFramework::Model::kOpenMenu:
			MCMBridge::QuickOpenWindow::Close();
			controller.OpenFrameworkView();
			break;
		case SKSEMenuFramework::Model::kBeforeRender:
			controller.BeginFrameworkFrame();
			break;
		case SKSEMenuFramework::Model::kAfterRender:
			controller.EndFrameworkFrame();
			break;
		case SKSEMenuFramework::Model::kCloseMenu:
			MCMBridge::QuickOpenWindow::Close();
			MCMBridge::MessageDialog::Cancel();
			controller.CloseFrameworkView();
			break;
		default:
			break;
		}
	}
}

namespace MCMBridge
{
	FrameworkApi& FrameworkApi::GetSingleton()
	{
		static FrameworkApi singleton;
		return singleton;
	}

	bool FrameworkApi::BindAndRegister()
	{
		if (registered) {
			return true;
		}
		const auto module = GetModuleHandleW(L"SKSEMenuFramework");
		if (!module) {
			SKSE::log::error("SKSE Menu Framework is not loaded; install release {} or newer", FrameworkRequirements::minimumRelease);
			return false;
		}

		if (const auto missing = FrameworkRequirements::FindMissingExport([module](const char* a_name) { return GetProcAddress(module, a_name) != nullptr; })) {
			SKSE::log::error("SKSE Menu Framework is missing required export {}; install release {} or newer from Nexus Mods", missing, FrameworkRequirements::minimumRelease);
			return false;
		}
		const auto apiVersion = SKSEMenuFramework::GetMenuFrameworkAPIVersion();
		if (!FrameworkRequirements::SupportsApi(apiVersion)) {
			SKSE::log::error(
				"SKSE Menu Framework API {} is unsupported; required API {} (release {} or newer)", apiVersion, FrameworkRequirements::minimumApiVersion, FrameworkRequirements::minimumRelease);
			return false;
		}
		version = SKSEMenuFramework::GetMenuFrameworkVersion();

		SKSEMenuFramework::SetSection("MCM Bridge");
		SKSEMenuFramework::AddSectionItem("Settings", SettingsWindow::Render);
		SKSEMenuFramework::AddSectionItem("Browser", BrowserWindow::Render);
		if (!WriteNotifications::Install() || !QuickOpenWindow::Install() || !MessageDialog::Install()) {
			SKSE::log::error("Failed to install setting change notifications");
			return false;
		}
		if (!KeybindSelector::Install()) {
			SKSE::log::error("Failed to install the keybind selector input handler");
			return false;
		}
		lifecycleRegistration.reset(SKSEMenuFramework::AddEvent(OnFrameworkEvent, 0.0F));
		if (!lifecycleRegistration) {
			SKSE::log::error("Failed to install the MCM host lifecycle handler");
			return false;
		}
		registered = true;
		SKSE::log::info("Registered MCM Bridge with SKSE Menu Framework API {}; legacy version report {} is not the release version (required release {} or newer)", apiVersion, version, FrameworkRequirements::minimumRelease);
		return true;
	}

	bool FrameworkApi::IsAvailable() const
	{
		return registered;
	}

	void FrameworkApi::SynchronizeMCMs(std::span<const MCMMod> a_mods)
	{
		if (registered) {
			MCMEntryRegistry::GetSingleton().Synchronize(a_mods);
		}
	}

	float FrameworkApi::Version() const
	{
		return version;
	}
}
