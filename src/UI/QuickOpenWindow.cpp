#include "MCMBridge/UI/QuickOpenWindow.h"
#include "MCMBridge/Core/QuickOpenPage.h"

#include "MCMBridge/Papyrus/NativeFacade.h"
#include "MCMBridge/Plugin/BridgeController.h"
#include "MCMBridge/Plugin/BridgeSettingsService.h"
#include "MCMBridge/UI/MCMWindow.h"
#include "SKSEMenuFramework.h"

namespace
{
	struct Target
	{
		std::uint64_t session{};
		std::string   mod;
		std::string   page;
	};
	std::atomic<std::shared_ptr<const Target>> target;
	std::mutex                                 targetMutex;
	SKSEMenuFramework::Model::WindowInterface* window{};

	void CloseIfCurrent(const std::shared_ptr<const Target>& a_expected)
	{
		const std::scoped_lock lock(targetMutex);
		if (target.load() != a_expected)
			return;
		if (window)
			window->IsOpen.store(false);
		target.store(nullptr);
	}

	void __stdcall Render()
	{
		const auto current = target.load();
		if (!current || current->session != MCMBridge::NativeFacadeSession().Session()) {
			CloseIfCurrent(current);
			return;
		}
		auto&      controller = MCMBridge::BridgeController::GetSingleton();
		const auto snapshot = controller.Snapshot();
		const auto mod = std::ranges::find(snapshot->mods, current->mod, &MCMBridge::MCMMod::stableID);
		bool       open = true;
		ImGuiMCP::SetNextWindowSize({ 1000.0F, 700.0F }, ImGuiMCP::ImGuiCond_FirstUseEver);
		if (ImGuiMCP::Begin("MCM Bridge - Requested menu", &open)) {
			if (mod == snapshot->mods.end()) {
				ImGuiMCP::TextWrapped("Waiting for the requested MCM navigation.");
			} else {
				const auto name = MCMBridge::ResolveMCMAlias(MCMBridge::BridgeSettingsService::GetSingleton().Get(), mod->stableID, mod->displayName);
				ImGuiMCP::TextUnformatted(name.c_str());
				const auto selected = MCMBridge::ResolveQuickOpenPage(*mod, current->page);
				if (selected)
					MCMBridge::MCMWindow::Render(mod->stableID, *selected);
				else {
					ImGuiMCP::TextWrapped("The requested page is missing or ambiguous. Refresh its navigation or choose the MCM in the browser.");
					if (ImGuiMCP::Button("Refresh navigation"))
						controller.RequestRefresh(true);
				}
			}
		}
		ImGuiMCP::End();
		if (!open)
			CloseIfCurrent(current);
	}
}

namespace MCMBridge::QuickOpenWindow
{
	bool Install()
	{
		if (!window)
			window = SKSEMenuFramework::AddWindow(Render, true);
		return window != nullptr;
	}
	void Open(std::uint64_t a_session, std::string a_modID, std::string a_page)
	{
		const std::scoped_lock lock(targetMutex);
		if (!window)
			return;
		target.store(std::make_shared<const Target>(Target{ a_session, std::move(a_modID), std::move(a_page) }));
		// Only one MCM view drives the hosted session in a render frame.
		if (auto* main = SKSEMenuFramework::GetMainWindow())
			main->IsOpen.store(false);
		window->IsOpen.store(true);
	}
	void Close()
	{
		const std::scoped_lock lock(targetMutex);
		if (window)
			window->IsOpen.store(false);
		target.store(nullptr);
	}
}
