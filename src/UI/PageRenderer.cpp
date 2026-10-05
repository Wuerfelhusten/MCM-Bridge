#include "MCMBridge/UI/PageRenderer.h"

#include "MCMBridge/UI/ControlRenderer.h"
#include "MCMBridge/UI/RichTextRenderer.h"
#include "SKSEMenuFramework.h"

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

		const auto flags = ImGuiMCP::ImGuiTableFlags_SizingStretchProp;
		if (!ImGuiMCP::BeginTable("MCMBridgePageColumns", 3, flags)) {
			return;
		}
		ImGuiMCP::TableSetupColumn("Left", ImGuiMCP::ImGuiTableColumnFlags_WidthStretch, 1.0F);
		ImGuiMCP::TableSetupColumn("Gutter", ImGuiMCP::ImGuiTableColumnFlags_WidthFixed, 40.0F);
		ImGuiMCP::TableSetupColumn("Right", ImGuiMCP::ImGuiTableColumnFlags_WidthStretch, 1.0F);
		const auto rowCount = (slotCount + 1) / 2;
		for (std::size_t row = 0; row < rowCount; ++row) {
			ImGuiMCP::TableNextRow(0, ImGuiMCP::GetFrameHeightWithSpacing());
			for (std::size_t column = 0; column < 2; ++column) {
				ImGuiMCP::TableSetColumnIndex(static_cast<int>(column * 2));
				if (const auto* control = slots[row * 2 + column]) {
					ControlRenderer::Render(a_snapshot, *control);
				}
			}
		}
		ImGuiMCP::EndTable();
	}
}
