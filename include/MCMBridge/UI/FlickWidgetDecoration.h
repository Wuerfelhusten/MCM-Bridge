#pragma once

#include <string_view>

namespace MCMBridge::FlickWidgetDecoration
{
	void Dropdown();
	void Icon(std::string_view a_id);
	// Release while the owning FLICK interface is still bound (render thread).
	void ReleaseIcon();
}
