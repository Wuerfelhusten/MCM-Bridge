#include "MCMBridge/UI/ControlRowRenderer.h"

#include "MCMBridge/Core/ControlRowLayout.h"
#include "MCMBridge/Core/SkyUIRichText.h"
#include "MCMBridge/UI/FrontendUI.h"
#include "MCMBridge/UI/RichTextRenderer.h"

namespace MCMBridge::ControlRowRenderer
{
	void BeginRow(std::string_view a_label, float a_preferredWidth, float a_reservedWidth)
	{
		const auto start = BridgeUI::GetCursorPosX();
		const auto width = BridgeUI::GetContentRegionAvail().x;
		const auto plain = PlainSkyUIText(a_label);
		const auto layout = FitControlRow(width, BridgeUI::CalcTextSize(plain.c_str()).x, a_preferredWidth, a_reservedWidth, BridgeUI::GetStyle()->ItemSpacing.x, plain.find('\n') != std::string::npos);
		if (!plain.empty()) {
			RichTextRenderer::Render(a_label);
			if (!layout.stacked)
				BridgeUI::SameLine();
		}
		BridgeUI::SetCursorPosX(start + layout.widgetOffset);
		BridgeUI::SetNextItemWidth((std::max)(1.0F, layout.widgetWidth));
	}
}
