#include "MCMBridge/UI/MCMWindow.h"
#include "MCMBridge/Core/CustomContentPolicy.h"

#include "MCMBridge/Plugin/BridgeController.h"
#include "MCMBridge/UI/FrontendUI.h"
#include "MCMBridge/UI/PageRenderer.h"

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
			BridgeUI::TextWrapped("This MCM closed its page.");
			if (BridgeUI::Button("Open page again"))
				controller.ClearHostedPageRoute();
			return;
		}
		if (routedPage != a_pageID) {
			BridgeUI::TextWrapped("This MCM selected another page.");
			if (BridgeUI::Button("Return to requested page")) {
				controller.ClearHostedPageRoute();
			} else {
				a_pageID = routedPage;
			}
		}
		controller.ObserveHostedPage(a_modID, a_pageID);
		if (!controller.IsHostedViewReady(a_modID, a_pageID)) {
			const auto error = controller.HostedViewError();
			BridgeUI::TextWrapped("%s", error.empty() ? "Loading current MCM page..." : error.c_str());
			if (!error.empty() && BridgeUI::Button("Retry"))
				controller.RetryHostedPage();
			return;
		}
		const auto  snapshot = controller.Snapshot();
		const auto* mod = FindMod(*snapshot, a_modID);
		const auto* page = mod ? FindPage(*mod, a_pageID) : nullptr;
		if (!mod || !page) {
			BridgeUI::TextDisabled("This MCM page is not available in the current game.");
			if (BridgeUI::Button("Refresh")) {
				controller.RequestRefresh();
			}
			BridgeUI::SameLine();
			if (BridgeUI::Button("Retry failed")) {
				controller.RetryFailed();
			}
			return;
		}

		if (requestedPage != a_pageID) {
			BridgeUI::TextUnformatted(page->displayName.c_str());
		}
		if (page->customContent) {
			controller.ObserveCustomContent(*mod, *page);
			BridgeUI::TextUnformatted(CustomContentPlaceholder(page->customContent->source).data());
			BridgeUI::TextWrapped("The native MCM host does not support rendering this custom content yet. This does not mean its source file is missing.");
			BridgeUI::TextWrapped("Source: %s", page->customContent->source.c_str());
			return;
		} else {
			controller.ObserveHostedPage(mod->stableID, page->stableID);
			if (page->controls.empty()) {
				BridgeUI::TextDisabled("This page contains no supported controls.");
			} else {
				PageRenderer::Render(*snapshot, *page);
			}
		}
		for (const auto& diagnostic : snapshot->diagnostics) {
			if (diagnostic.sourceID == mod->stableID) {
				BridgeUI::TextDisabled("%s", diagnostic.message.c_str());
			}
		}
	}
}
