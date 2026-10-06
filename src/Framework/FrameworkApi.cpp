#include "MCMBridge/Framework/FrameworkApi.h"
#include "MCMBridge/Framework/FlickApi.h"
#include "MCMBridge/Framework/FrameworkRequirements.h"
#include "MCMBridge/Framework/FrontendWindow.h"
#include "MCMBridge/Framework/MCMEntryRegistry.h"
#include "MCMBridge/Framework/RenderContext.h"
#include "MCMBridge/Plugin/BridgeController.h"
#include "MCMBridge/Plugin/BridgeSettingsService.h"
#include "MCMBridge/Plugin/TaskScheduler.h"
#include "MCMBridge/UI/BrowserWindow.h"
#include "MCMBridge/UI/KeybindSelector.h"
#include "MCMBridge/UI/MessageDialog.h"
#include "MCMBridge/UI/QuickOpenWindow.h"
#include "MCMBridge/UI/SettingsWindow.h"
#include "MCMBridge/UI/WriteNotifications.h"
#include "SKSEMenuFramework.h"

namespace
{
	using namespace MCMBridge;
	std::unique_ptr<SKSEMenuFramework::Model::Event> lifecycleRegistration;
	void __stdcall                                   Settings()
	{
		const std::scoped_lock lock(FrameworkApi::RenderMutex());
		if (!FrameworkApi::GetSingleton().CanRender(Frontend::kMenuFramework))
			return;
		RenderContext context(Frontend::kMenuFramework);
		SettingsWindow::Render();
	}
	void __stdcall Browser()
	{
		const std::scoped_lock lock(FrameworkApi::RenderMutex());
		if (!FrameworkApi::GetSingleton().CanRender(Frontend::kMenuFramework))
			return;
		RenderContext context(Frontend::kMenuFramework);
		BrowserWindow::Render();
	}
	void __stdcall OnFrameworkEvent(SKSEMenuFramework::Model::EventType a_eventType)
	{
		const std::scoped_lock lock(FrameworkApi::RenderMutex());
		if (!FrameworkApi::GetSingleton().CanRender(Frontend::kMenuFramework))
			return;
		auto& controller = BridgeController::GetSingleton();
		switch (a_eventType) {
		case SKSEMenuFramework::Model::kOpenMenu:
			QuickOpenWindow::Close();
			controller.OpenFrameworkView();
			break;
		case SKSEMenuFramework::Model::kBeforeRender:
			if (!FrontendWindow::PrimaryOpen())
				controller.BeginFrameworkFrame();
			break;
		case SKSEMenuFramework::Model::kAfterRender:
			if (!FrontendWindow::PrimaryOpen())
				controller.EndFrameworkFrame();
			break;
		case SKSEMenuFramework::Model::kCloseMenu:
			if (!FrontendWindow::PrimaryOpen()) {
				MessageDialog::Cancel();
				controller.CloseFrameworkView();
			}
			break;
		default:
			break;
		}
	}
	bool BindMenuFramework(float& a_version)
	{
		const auto module = GetModuleHandleW(L"SKSEMenuFramework");
		if (!module)
			return false;
		if (const auto missing = FrameworkRequirements::FindMissingExport([module](const char* a_name) { return GetProcAddress(module, a_name) != nullptr; })) {
			SKSE::log::error("SKSE Menu Framework is missing {}; install release {} or newer", missing, FrameworkRequirements::minimumRelease);
			return false;
		}
		const auto apiVersion = SKSEMenuFramework::GetMenuFrameworkAPIVersion();
		if (!FrameworkRequirements::SupportsApi(apiVersion)) {
			SKSE::log::error("Unsupported SKSE Menu Framework API {}; required release {} or newer", apiVersion, FrameworkRequirements::minimumRelease);
			return false;
		}
		a_version = SKSEMenuFramework::GetMenuFrameworkVersion();
		SKSE::log::info("Connected SKSE Menu Framework API {}; legacy version {} is not the release version", apiVersion, a_version);
		return true;
	}
}

namespace MCMBridge
{
	FrameworkApi& FrameworkApi::GetSingleton()
	{
		static FrameworkApi singleton;
		return singleton;
	}
	std::recursive_mutex& FrameworkApi::RenderMutex()
	{
		static std::recursive_mutex mutex;
		return mutex;
	}
	bool FrameworkApi::BindAndRegister()
	{
		if (registered)
			return true;
		menuFramework = BindMenuFramework(version);
		flick = FlickApi::Bind();
		active.store(SelectFrontend(flick, menuFramework, BridgeSettingsService::GetSingleton().Get().preferFlick));
		if (Active() == Frontend::kNone) {
			SKSE::log::error("No compatible frontend; install FLICK or SKSE Menu Framework {} or newer", FrameworkRequirements::minimumRelease);
			return false;
		}
		if (flick)
			FlickApi::Install();
		if (Active() == Frontend::kMenuFramework)
			RegisterMenuFramework();
		if (!WriteNotifications::Install() || !QuickOpenWindow::Install() || !MessageDialog::Install() || !KeybindSelector::Install())
			return false;
		if (menuFramework) {
			lifecycleRegistration.reset(SKSEMenuFramework::AddEvent(OnFrameworkEvent, 0));
			if (!lifecycleRegistration)
				return false;
		}
		registered = true;
		SKSE::log::info("MCM frontend: {}", Active() == Frontend::kFlick ? "FLICK" : "SKSE Menu Framework");
		return true;
	}
	void FrameworkApi::RegisterMenuFramework()
	{
		SKSEMenuFramework::SetSection("MCM Bridge");
		SKSEMenuFramework::AddSectionItem("Settings", Settings);
		SKSEMenuFramework::AddSectionItem("Browser", Browser);
	}
	bool     FrameworkApi::IsAvailable() const { return registered; }
	float    FrameworkApi::Version() const { return version; }
	Frontend FrameworkApi::Active() const { return active.load(); }
	bool     FrameworkApi::HasMenuFramework() const { return menuFramework; }
	bool     FrameworkApi::HasFlick() const { return flick; }
	bool     FrameworkApi::CanRender(Frontend a_frontend) const { return CanRenderAuxiliary(a_frontend) && !switching.load(); }
	bool     FrameworkApi::CanRenderAuxiliary(Frontend a_frontend) const { return registered.load() && Active() == a_frontend; }
	void     FrameworkApi::SynchronizeMCMs(std::span<const MCMMod> a_mods)
	{
		const std::scoped_lock lock(RenderMutex());
		if (!registered || switching.load())
			return;
		if (Active() == Frontend::kFlick)
			FlickApi::Synchronize(a_mods);
		else
			MCMEntryRegistry::GetSingleton().Synchronize(a_mods);
	}
	void FrameworkApi::SetOpen(bool a_open)
	{
		const std::scoped_lock lock(RenderMutex());
		if (!registered || switching.load())
			return;
		if (Active() == Frontend::kFlick)
			FlickApi::SetOpen(a_open);
		else if (auto* window = SKSEMenuFramework::GetMainWindow())
			window->IsOpen.store(a_open);
	}
	bool FrameworkApi::IsOpen() const
	{
		if (!registered)
			return false;
		if (Active() == Frontend::kFlick)
			return FlickApi::IsOpen();
		const auto* window = SKSEMenuFramework::GetMainWindow();
		return window && window->IsOpen.load();
	}
	void FrameworkApi::RequestPreferenceSwitch()
	{
		// Game task only. Repeated changes share one retry chain.
		if (!registered || switchQueued)
			return;
		switchQueued = true;
		DrivePreferenceSwitch();
	}
	void FrameworkApi::InvalidateHandoff()
	{
		const std::scoped_lock lock(RenderMutex());
		++switchEpoch;
		switching.store(false);
		switchQueued = false;
		RequestPreferenceSwitch();
	}
	void FrameworkApi::DrivePreferenceSwitch()
	{
		const std::scoped_lock lock(RenderMutex());
		const auto             wanted = SelectFrontend(flick, menuFramework, BridgeSettingsService::GetSingleton().Get().preferFlick);
		const auto             epoch = switchEpoch;
		if (wanted == Active()) {
			switchQueued = false;
			return;
		}
		if (switching.load() || MessageDialog::IsPending() || KeybindSelector::IsCapturingAny() ||
			!BridgeController::GetSingleton().TryFrontendHandoff([this, epoch] {
				const std::scoped_lock completionLock(RenderMutex());
				if (epoch != switchEpoch)
					return;
				if (Active() == Frontend::kMenuFramework) {
					if (auto* window = SKSEMenuFramework::GetMainWindow())
						window->IsOpen.store(false);
					MCMEntryRegistry::GetSingleton().Deactivate();
					SKSEMenuFramework::DeleteSection("MCM Bridge");
				} else
					FlickApi::SetOpen(false);
				QuickOpenWindow::Close();
				active.store(SelectFrontend(flick, menuFramework, BridgeSettingsService::GetSingleton().Get().preferFlick));
				if (Active() == Frontend::kMenuFramework)
					RegisterMenuFramework();
				switching.store(false);
				FrontendWindow::RefreshBackend();
				switchQueued = false;
				SynchronizeMCMs(BridgeController::GetSingleton().Snapshot()->mods);
				SetOpen(true);
				SKSE::log::info("MCM frontend switched to {} after safe host cleanup", Active() == Frontend::kFlick ? "FLICK" : "SKSE Menu Framework");
			})) {
			TaskScheduler::GetSingleton().After(std::chrono::milliseconds(100), [this, epoch] { if (epoch == switchEpoch) DrivePreferenceSwitch(); });
			return;
		}
		// Completion is queued even when no session needs closing.
		switching.store(true);
	}
}
