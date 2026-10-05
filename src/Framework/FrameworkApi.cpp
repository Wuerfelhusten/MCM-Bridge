#include "MCMBridge/Framework/FrameworkApi.h"

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
	constexpr float                                  minimumFrameworkVersion = 3.8F;
	constexpr std::uint32_t                          minimumFrameworkApiVersion = 1U;
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
		if (!GetModuleHandleW(L"SKSEMenuFramework")) {
			SKSE::log::error("SKSE Menu Framework is not loaded");
			return false;
		}

		version = SKSEMenuFramework::GetMenuFrameworkVersion();
		if (version < minimumFrameworkVersion) {
			SKSE::log::error("SKSE Menu Framework {} is older than required version {}", version, minimumFrameworkVersion);
			return false;
		}
		const auto apiVersion = SKSEMenuFramework::GetMenuFrameworkAPIVersion();
		if (apiVersion < minimumFrameworkApiVersion) {
			SKSE::log::error(
				"SKSE Menu Framework API {} is older than required API {}", apiVersion, minimumFrameworkApiVersion);
			return false;
		}

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
		SKSE::log::info("Registered MCM Bridge with SKSE Menu Framework {}", version);
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
