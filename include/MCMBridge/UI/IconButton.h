#pragma once

#include <string_view>

namespace MCMBridge::IconButton
{
	// Keeps the normal button interaction and last-item state for tooltips.
	bool Render(std::string_view a_icon, std::string_view a_id, std::string_view a_text = {});
}
