#include "MCMBridge/UI/MCMWindow.h"
#include "MCMBridge/Core/CustomContentPolicy.h"

#include "MCMBridge/Plugin/BridgeController.h"
#include "MCMBridge/UI/IconButton.h"
#include "MCMBridge/UI/PageRenderer.h"
#include "SKSEMenuFramework.h"

namespace
{
	const MCMBridge::MCMMod* FindMod(const MCMBridge::MCMSnapshot& a_snapshot, std::string_view a_modID)
	{
		const auto found = std::ranges::find(a_snapshot.mods, a_modID, &MCMBridge::MCMMod::stableID);
		return found != a_snapshot.mods.end() ? std::addressof(*found) : nullptr;
	}

	const MCMBridge::MCMPage* FindPage(const MCMBridge::MCMMod& a_mod, std::string_view a_pageID)
	{
		const auto found = std::ranges::find(a_mod.pages, a_pageID, &MCMBridge::MCMPage::stableID);
		return found != a_mod.pages.end() ? std::addressof(*found) : nullptr;
	}
}

namespace MCMBridge::MCMWindow
{
	void Render(std::string_view a_modID, std::string_view a_pageID)
	{
		auto&      controller = BridgeController::GetSingleton();
		const auto requestedPage = a_pageID;
		const auto routedPage = controller.ResolveHostedPage(a_modID, a_pageID);
		if (routedPage.empty()) {
			ImGuiMCP::TextWrapped("This MCM closed its page.");
			if (ImGuiMCP::Button("Open page again"))
				controller.ClearHostedPageRoute();
			return;
		}
		if (routedPage != a_pageID) {
			ImGuiMCP::TextWrapped("This MCM selected another page.");
			if (ImGuiMCP::Button("Return to requested page")) {
				controller.ClearHostedPageRoute();
			} else {
				a_pageID = routedPage;
			}
		}
		controller.ObserveHostedPage(a_modID, a_pageID);
		if (!controller.IsHostedViewReady(a_modID, a_pageID)) {
			const auto error = controller.HostedViewError();
			ImGuiMCP::TextWrapped("%s", error.empty() ? "Loading current MCM page..." : error.c_str());
			if (!error.empty() && ImGuiMCP::Button("Retry"))
				controller.RetryHostedPage();
			return;
		}
		const auto  snapshot = controller.Snapshot();
		const auto* mod = FindMod(*snapshot, a_modID);
		const auto* page = mod ? FindPage(*mod, a_pageID) : nullptr;
		if (!mod || !page) {
			ImGuiMCP::TextDisabled("This MCM page is not available in the current game.");
			if (ImGuiMCP::Button("Refresh")) {
				controller.RequestRefresh();
			}
			ImGuiMCP::SameLine();
			if (ImGuiMCP::Button("Retry failed")) {
				controller.RetryFailed();
			}
			return;
		}

		if (requestedPage != a_pageID) {
			ImGuiMCP::TextUnformatted(page->displayName.c_str());
		}
		if (page->customContent) {
			if (controller.IsNativeHost()) {
				controller.ObserveCustomContent(*mod, *page);
				ImGuiMCP::TextUnformatted(CustomContentPlaceholder(page->customContent->source).data());
				ImGuiMCP::TextWrapped("The native MCM host does not support rendering this custom content yet. This does not mean its source file is missing.");
				ImGuiMCP::TextWrapped("Source: %s", page->customContent->source.c_str());
				return;
			}
			ImGuiMCP::TextWrapped("This page uses custom content provided by the original MCM.");
			static const auto openIcon = FontAwesome::UnicodeToUtf8(0xf35d);
			const auto        buttonID = std::format("original-{}:{}", mod->stableID, page->stableID);
			const auto        open = IconButton::Render(openIcon, buttonID, "Open original");
			if (open) {
				controller.OpenOriginal(mod->stableID, page->stableID);
			}
		} else {
			controller.ObserveHostedPage(mod->stableID, page->stableID);
			if (page->controls.empty()) {
				ImGuiMCP::TextDisabled("This page contains no supported controls.");
			} else {
				PageRenderer::Render(*snapshot, *page);
			}
		}
		for (const auto& diagnostic : snapshot->diagnostics) {
			if (diagnostic.sourceID == mod->stableID) {
				ImGuiMCP::TextDisabled("%s", diagnostic.message.c_str());
			}
		}
	}
}
