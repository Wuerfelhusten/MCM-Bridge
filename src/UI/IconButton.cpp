#include "MCMBridge/UI/IconButton.h"

#include "SKSEMenuFramework.h"

#include <format>

namespace MCMBridge::IconButton
{
	bool Render(std::string_view a_icon, std::string_view a_id, std::string_view a_text)
	{
		const auto label = std::format("{}{}{}##{}", a_icon, a_text.empty() ? "" : " ", a_text, a_id);
		const auto height = ImGuiMCP::GetFrameHeight();
		const auto padding = ImGuiMCP::GetStyle()->FramePadding;
		const auto size = ImGuiMCP::ImVec2{ a_text.empty() ? height : 0.0F, height };

		// Text padding can exceed the free space around an icon in a square button.
		ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_FramePadding,
			ImGuiMCP::ImVec2{ a_text.empty() ? 0.0F : padding.x, padding.y });
		ImGuiMCP::PushStyleVar(ImGuiMCP::ImGuiStyleVar_ButtonTextAlign, ImGuiMCP::ImVec2{ 0.5F, 0.5F });
		const auto clicked = ImGuiMCP::Button(label.c_str(), size);
		ImGuiMCP::PopStyleVar(2);
		return clicked;
	}
}
