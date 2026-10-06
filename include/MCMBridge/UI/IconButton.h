#pragma once

#include <string_view>

namespace MCMBridge::IconButton
{
	// Keeps the normal button interaction and last-item state for tooltips.
	bool Render(std::string_view a_icon, std::string_view a_id, std::string_view a_text = {});
	// Place an action against an actual widget rectangle, not its text baseline.
	void AlignToLastWidget(float a_rightEdge);
}
