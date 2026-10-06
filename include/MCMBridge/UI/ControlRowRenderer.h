#pragma once

#include <string_view>

namespace MCMBridge::ControlRowRenderer
{
	void BeginRow(std::string_view a_label, float a_preferredWidth, float a_reservedWidth);
}
