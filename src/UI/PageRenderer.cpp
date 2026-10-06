#include "MCMBridge/UI/PageRenderer.h"

#include "MCMBridge/UI/ControlRenderer.h"
#include "MCMBridge/UI/FrontendUI.h"
#include "MCMBridge/UI/RichTextRenderer.h"

#include <array>

namespace MCMBridge::PageRenderer
{
	void Render(const MCMSnapshot& a_snapshot, const MCMPage& a_page)
	{
		if (!a_page.title.empty() && a_page.title != a_page.displayName) {
			RichTextRenderer::RenderHeader(a_page.title);
		}
		constexpr std::size_t                       maximumSlots = 128;
		std::array<const MCMControl*, maximumSlots> slots{};
		std::size_t                                 slotCount{};
		for (const auto& control : a_page.controls) {
			if (control.layout.position < 0 || control.layout.position >= static_cast<std::int32_t>(maximumSlots)) {
				continue;
			}
			const auto position = static_cast<std::size_t>(control.layout.position);
			slots[position] = std::addressof(control);
			slotCount = (std::max)(slotCount, position + 1);
		}

		const auto flick = renderFrontend == Frontend::kFlick;
		const auto compact = flick && BridgeUI::GetContentRegionAvail().x < BridgeUI::GetFontSize() * 24;
		const auto flags = BridgeUI::ImGuiTableFlags_SizingStretchProp;
		if (!BridgeUI::BeginTable("MCMBridgePageColumns", compact ? 1 : 3, flags)) {
			return;
		}
		BridgeUI::TableSetupColumn("Left", BridgeUI::ImGuiTableColumnFlags_WidthStretch, 1.0F);
		if (!compact) {
			BridgeUI::TableSetupColumn("Gutter", BridgeUI::ImGuiTableColumnFlags_WidthFixed, flick ? BridgeUI::GetFontSize() : 40.0F);
			BridgeUI::TableSetupColumn("Right", BridgeUI::ImGuiTableColumnFlags_WidthStretch, 1.0F);
		}
		const auto rowCount = (slotCount + 1) / 2;
		for (std::size_t row = 0; row < rowCount; ++row) {
			BridgeUI::TableNextRow(0, BridgeUI::GetFrameHeightWithSpacing());
			for (std::size_t column = 0; column < 2; ++column) {
				if (compact && column)
					BridgeUI::TableNextRow(0, BridgeUI::GetFrameHeightWithSpacing());
				BridgeUI::TableSetColumnIndex(compact ? 0 : static_cast<int>(column * 2));
				if (const auto* control = slots[row * 2 + column]) {
					ControlRenderer::Render(a_snapshot, *control);
				}
			}
		}
		BridgeUI::EndTable();
	}
}
