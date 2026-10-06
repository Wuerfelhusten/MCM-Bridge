#include "MCMBridge/Core/ControlRowLayout.h"
#include "MCMBridge/Core/FrontendSelection.h"
#include "MCMBridge/Core/RichTextLayout.h"
#include "MCMBridge/Framework/FlickApi.h"
#include "MCMBridge/Plugin/BridgeSettingsService.h"
#include "MCMBridge/UI/FrontendUI.h"
#include "MCMBridge/UI/MCMWindow.h"

// Declare frontend-local value types before the host's global ImGui types.
#include "FUCK_API.h"

namespace
{
	using namespace MCMBridge;

	void PageEntries(const MCMMod& a_mod, std::string& a_page, int a_columns)
	{
		auto& api = *FUCK::GetInterface();
		api.TextUnformatted("Pages", nullptr);
		api.Separator();
		if (api.BeginTable("MCMBridgePages", a_columns, ImGuiTableFlags_SizingStretchSame, {}, 0)) {
			for (int column = 0; column < a_columns; ++column)
				api.TableSetupColumn("Page", ImGuiTableColumnFlags_WidthStretch, 1, 0);
			std::size_t position{};
			for (const auto& entry : a_mod.pages) {
				const auto column = static_cast<int>(position++ % static_cast<std::size_t>(a_columns));
				if (!column)
					api.TableNextRow(0, api.GetFrameHeightWithSpacing());
				api.TableSetColumnIndex(column);
				float width, height;
				api.GetContentRegionAvail(&width, &height);
				const auto  fullLabel = FlickPageLabel(entry);
				const auto  lines = WrapRichText(SkyUIRichText{ fullLabel, { { fullLabel, std::nullopt } } }, (std::max)(1.0F, width), [&api](std::string_view a_text) {
					float x, y;
					api.CalcTextSize(a_text.data(), a_text.data() + a_text.size(), false, -1, &x, &y);
					return x;
				});
				std::string display;
				for (const auto& line : lines) {
					if (!display.empty())
						display += '\n';
					for (const auto& span : line)
						display += span.text;
				}
				api.PushID_Str(entry.stableID.c_str());
				const auto label = display + "###page";
				const auto itemHeight = (std::max)(api.GetFrameHeight(), api.GetTextLineHeight() * static_cast<float>(lines.size()));
				if (api.Selectable(label.c_str(), entry.stableID == a_page, 0, { width, itemHeight }))
					a_page = entry.stableID;
				if (api.IsItemHovered(0))
					api.SetTooltip(fullLabel.c_str());
				api.PopID();
			}
			api.EndTable();
		}
	}

	void PageDropdown(const MCMMod& a_mod, std::string& a_page)
	{
		const auto               selected = std::ranges::find(a_mod.pages, a_page, &MCMPage::stableID);
		auto                     index = selected == a_mod.pages.end() ? -1 : static_cast<int>(selected - a_mod.pages.begin());
		std::vector<std::string> labels;
		std::vector<const char*> items;
		labels.reserve(a_mod.pages.size());
		for (const auto& entry : a_mod.pages)
			labels.push_back(FlickPageLabel(entry));
		for (const auto& label : labels)
			items.push_back(label.c_str());
		BridgeUI::SetNextItemWidth(BridgeUI::GetContentRegionAvail().x);
		if (BridgeUI::Combo("##MCMBridgePageDropdown", &index, items.data(), static_cast<int>(items.size())) && index >= 0 && static_cast<std::size_t>(index) < a_mod.pages.size())
			a_page = a_mod.pages[static_cast<std::size_t>(index)].stableID;
	}

	void PageContent(const MCMMod& a_mod, const std::string& a_page)
	{
		FUCK::BeginPanelFrame("MCMBridgePageContent");
		auto& api = *FUCK::GetInterface();
		float width, height;
		api.GetContentRegionAvail(&width, &height);
		const auto fontSize = api.GetTextLineHeight();
		auto*      font = api.GetFont(FUCK::Font::kRegular);
		if (font)
			api.PushFont(font, fontSize * FitMCMContentScale(width, fontSize));
		// A removed page must not silently select another page at its old index.
		if (std::ranges::find(a_mod.pages, a_page, &MCMPage::stableID) != a_mod.pages.end())
			MCMWindow::Render(a_mod.stableID, a_page);
		else
			FUCK::GetInterface()->TextWrapped("The selected page is no longer available. Choose a page in the selector.");
		if (font)
			api.PopFont();
		FUCK::EndPanelFrame();
	}
}

namespace MCMBridge::FlickApi
{
	void RenderPages(const MCMMod& a_mod, std::string& a_page)
	{
		auto& api = *FUCK::GetInterface();
		if (a_page.empty() && !a_mod.pages.empty())
			a_page = a_mod.pages.front().stableID;
		const auto settings = BridgeSettingsService::GetSingleton().Get();
		float      width, height;
		api.GetContentRegionAvail(&width, &height);
		if (settings.flickPageSelector == FlickPageSelector::kLeft && width >= api.GetTextLineHeight() * 12) {
			if (api.BeginTable("MCMBridgePageLayout", 2, ImGuiTableFlags_BordersInnerV, {}, 0)) {
				api.TableSetupColumn("Pages", ImGuiTableColumnFlags_WidthFixed, width * static_cast<float>(settings.flickPageWidth) / 100, 0);
				api.TableSetupColumn("Content", ImGuiTableColumnFlags_WidthStretch, 1, 0);
				api.TableNextRow(0, 0);
				api.TableSetColumnIndex(0);
				FUCK::BeginPanelFrame("MCMBridgePageSelectorFrame");
				PageEntries(a_mod, a_page, 1);
				FUCK::EndPanelFrame();
				api.TableSetColumnIndex(1);
				PageContent(a_mod, a_page);
				api.EndTable();
			}
			return;
		}
		if (settings.flickPageSelector == FlickPageSelector::kDropdown) {
			PageDropdown(a_mod, a_page);
		} else {
			const auto frame = api.GetFrameHeightWithSpacing();
			const auto columns = width >= api.GetTextLineHeight() * 30 ? 3 : width >= api.GetTextLineHeight() * 15 ? 2 :
			                                                                                                         1;
			const auto rows = (a_mod.pages.size() + static_cast<std::size_t>(columns) - 1) / static_cast<std::size_t>(columns);
			const auto panelHeight = (std::min)(height * 0.4F, frame * static_cast<float>(1 + (std::min)(rows, static_cast<std::size_t>(settings.flickPageRows))) + FUCK::Scale(24));
			// Bound the navigation panel first: BeginPanelFrame otherwise consumes
			// all remaining height. Its inner child owns scrolling and clipping.
			api.BeginChild("MCMBridgePageSelector", 0, panelHeight, false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
			FUCK::BeginPanelFrame("MCMBridgePageSelectorFrame");
			PageEntries(a_mod, a_page, columns);
			FUCK::EndPanelFrame();
			api.EndChild();
		}
		api.Separator();
		PageContent(a_mod, a_page);
	}
}
