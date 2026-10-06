#include "MCMBridge/UI/IconButton.h"

#include "MCMBridge/UI/FlickWidgetDecoration.h"
#include "MCMBridge/UI/FrontendUI.h"

// Declare the frontend-local value types before global ImGui declarations.
#include "FUCK_API.h"
#include "imgui_internal.h"

#include <format>

namespace MCMBridge::IconButton
{
	void AlignToLastWidget(float a_rightEdge)
	{
		if (renderFrontend != Frontend::kFlick) {
			BridgeUI::SameLine();
			return;
		}
		auto&  api = *FUCK::GetInterface();
		ImVec2 minimum, maximum, cursor, screen;
		api.GetItemRectMin(&minimum.x, &minimum.y);
		api.GetItemRectMax(&maximum.x, &maximum.y);
		api.GetCursorPos(&cursor.x, &cursor.y);
		api.GetCursorScreenPos(&screen.x, &screen.y);
		const auto size = BridgeUI::GetFrameHeight();
		BridgeUI::SameLine();
		api.SetCursorPos(a_rightEdge - size, minimum.y - (screen.y - cursor.y) + (maximum.y - minimum.y - size) * 0.5F);
	}

	bool Render(std::string_view a_icon, std::string_view a_id, std::string_view a_text)
	{
		const auto drawnIcon = renderFrontend == Frontend::kFlick && a_text.empty() && (a_id.starts_with("reset-") || a_id.starts_with("clear-"));
		// The reset texture uses the same glyph as SMF, owned by FLICK rather than
		// borrowing an incompatible font atlas.
		const auto icon = renderFrontend == Frontend::kFlick ?
		                      (drawnIcon ? std::string_view{} : std::string_view("...")) :
		                      a_icon;
		const auto label = std::format("{}{}{}##{}", icon, a_text.empty() ? "" : " ", a_text, a_id);
		const auto height = BridgeUI::GetFrameHeight();
		const auto padding = BridgeUI::GetStyle()->FramePadding;
		const auto size = BridgeUI::ImVec2{ a_text.empty() ? height : 0.0F, height };

		// Text padding can exceed the free space around an icon in a square button.
		BridgeUI::PushStyleVar(BridgeUI::ImGuiStyleVar_FramePadding,
			BridgeUI::ImVec2{ a_text.empty() ? 0.0F : padding.x, padding.y });
		BridgeUI::PushStyleVar(BridgeUI::ImGuiStyleVar_ButtonTextAlign, BridgeUI::ImVec2{ 0.5F, 0.5F });
		bool clicked;
		if (drawnIcon) {
			// Selectable adds CurrLineTextBaseOffset after SameLine, shifting icon
			// actions below padded dropdowns. InvisibleButton keeps exact bounds
			// and still participates in keyboard/controller navigation.
			auto& api = *FUCK::GetInterface();
			clicked = api.InvisibleButton(label.c_str(), { height, height }, ImGuiButtonFlags_EnableNav);
			ImVec2 minimum, maximum;
			ImVec4 color;
			api.GetItemRectMin(&minimum.x, &minimum.y);
			api.GetItemRectMax(&maximum.x, &maximum.y);
			api.GetStyleColorVec4(api.IsItemHovered(0) ? ImGuiCol_Text : ImGuiCol_Border, &color.x, &color.y, &color.z, &color.w);
			color.w *= api.GetStyleVar(ImGuiStyleVar_Alpha);
			api.DrawRect({ minimum.x + 0.5F, minimum.y + 0.5F }, { maximum.x - 0.5F, maximum.y - 0.5F }, color, 2, 1);
		} else {
			clicked = BridgeUI::Button(label.c_str(), size);
		}
		BridgeUI::PopStyleVar(2);
		if (drawnIcon)
			FlickWidgetDecoration::Icon(a_id);
		return clicked;
	}
}
