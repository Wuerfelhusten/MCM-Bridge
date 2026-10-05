#pragma once

#include "MCMBridge/Core/SkyUIRichText.h"

namespace MCMBridge::RichTextRenderer
{
	void Render(std::string_view a_source);
	void RenderDisabled(const SkyUIRichText& a_text);
	void RenderHeader(std::string_view a_source);
}
