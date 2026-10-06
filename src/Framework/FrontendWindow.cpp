#include "MCMBridge/Framework/FrontendWindow.h"
#include "MCMBridge/Framework/FlickApi.h"
#include "MCMBridge/Framework/FrameworkApi.h"
#include "MCMBridge/Framework/RenderContext.h"
#include "MCMBridge/Framework/RendererCallbacks.h"
#include "MCMBridge/Plugin/BridgeController.h"
#include "MCMBridge/UI/KeybindSelector.h"
#include "SKSEMenuFramework.h"

namespace
{
	std::vector<std::unique_ptr<MCMBridge::FrontendWindow>> windows;
	void                                                    Draw(std::size_t a_slot)
	{
		const std::scoped_lock lock(MCMBridge::FrameworkApi::RenderMutex());
		if (a_slot >= windows.size())
			return;
		const auto& framework = MCMBridge::FrameworkApi::GetSingleton();
		if (!(windows[a_slot]->primary ? framework.CanRender(MCMBridge::Frontend::kMenuFramework) : framework.CanRenderAuxiliary(MCMBridge::Frontend::kMenuFramework)))
			return;
		MCMBridge::RenderContext context(MCMBridge::Frontend::kMenuFramework);
		windows[a_slot]->Render();
	}
	MCMBridge::RendererCallbacks callbacks(Draw);
}

namespace MCMBridge
{
	FrontendWindow* FrontendWindow::Create(std::string a_id, std::string a_title, Renderer a_render, bool a_blocking,
		float a_width, float a_height, bool a_primary, std::function<void()> a_closed)
	{
		const std::scoped_lock lock(FrameworkApi::RenderMutex());
		auto                   window = std::make_unique<FrontendWindow>();
		window->id = std::move(a_id);
		window->title = std::move(a_title);
		window->renderer = a_render;
		window->blocking = a_blocking;
		window->primary = a_primary;
		window->width = a_width;
		window->height = a_height;
		window->closed = std::move(a_closed);
		if (FrameworkApi::GetSingleton().HasMenuFramework())
			window->frameworkWindow = SKSEMenuFramework::AddWindow(callbacks.Get(windows.size()), a_blocking);
		if (FrameworkApi::GetSingleton().HasFlick())
			FlickApi::RegisterWindow(*window);
		const auto result = window.get();
		windows.push_back(std::move(window));
		return result;
	}
	void FrontendWindow::SetOpen(bool a_open)
	{
		const auto changed = open.exchange(a_open) != a_open;
		if (primary && changed) {
			if (a_open)
				BridgeController::GetSingleton().OpenFrameworkView();
			else
				BridgeController::GetSingleton().CloseFrameworkView();
		}
		if (frameworkWindow) {
			const auto& framework = FrameworkApi::GetSingleton();
			// CloseConfig may open a message during handoff. Keep that dialog usable
			// while ordinary page mutations are suspended until cleanup completes.
			const auto visible = primary ? framework.CanRender(Frontend::kMenuFramework) : framework.CanRenderAuxiliary(Frontend::kMenuFramework);
			static_cast<SKSEMenuFramework::Model::WindowInterface*>(frameworkWindow)->IsOpen.store(a_open && visible);
		}
	}
	void FrontendWindow::UserClose()
	{
		if (!open.exchange(false))
			return;
		SetOpen(false);
		if (closed)
			closed();
		if (primary)
			BridgeController::GetSingleton().CloseFrameworkView();
	}
	void FrontendWindow::Render()
	{
		if (!IsOpen())
			return;
		if (primary) {
			BridgeController::GetSingleton().BeginFrameworkFrame();
			KeybindSelector::BeginFrame();
		}
		renderer();
		if (frameworkWindow && renderFrontend == Frontend::kMenuFramework &&
			!static_cast<SKSEMenuFramework::Model::WindowInterface*>(frameworkWindow)->IsOpen.load())
			UserClose();
		if (primary) {
			KeybindSelector::EndFrame();
			BridgeController::GetSingleton().EndFrameworkFrame();
		}
	}
	void FrontendWindow::CloseAll()
	{
		for (auto& window : windows) window->SetOpen(false);
	}
	void FrontendWindow::RefreshBackend()
	{
		// Transfer notifications without losing pending contents. Message dialogs
		// finish on the old frontend; requested MCM windows close beforehand.
		for (auto& window : windows) window->SetOpen(window->IsOpen());
	}
	bool FrontendWindow::PrimaryOpen()
	{
		return std::ranges::any_of(windows, [](const auto& a_window) { return a_window->primary && a_window->IsOpen(); });
	}
}
