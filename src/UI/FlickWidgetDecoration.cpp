#include "MCMBridge/UI/FlickWidgetDecoration.h"

#include "MCMBridge/UI/FrontendUI.h"

// Use only FLICK's current-window drawing API, never an SMF draw list/font.
#include "FUCK_API.h"

namespace
{
	bool         imageAttempted{};
	FUCK::Image& ResetImage()
	{
		// FLICK owns this opaque image handle and its texture. One filtered image
		// avoids the internal anti-alias seams from independently drawn triangles.
		static FUCK::Image image;
		return image;
	}

	ImVec4 WidgetColor(ImGuiCol a_color)
	{
		auto&  api = *FUCK::GetInterface();
		ImVec4 result;
		api.GetStyleColorVec4(a_color, &result.x, &result.y, &result.z, &result.w);
		result.w *= api.GetStyleVar(ImGuiStyleVar_Alpha);
		return result;
	}

	std::pair<ImVec2, ImVec2> Bounds()
	{
		auto&  api = *FUCK::GetInterface();
		ImVec2 minimum, maximum;
		api.GetItemRectMin(&minimum.x, &minimum.y);
		api.GetItemRectMax(&maximum.x, &maximum.y);
		return { minimum, maximum };
	}
}

namespace MCMBridge::FlickWidgetDecoration
{
	void Dropdown()
	{
		auto& api = *FUCK::GetInterface();
		const auto [minimum, maximum] = Bounds();
		const auto   height = maximum.y - minimum.y;
		const auto   radius = height * 0.13F;
		const ImVec2 center{ maximum.x - height * 0.5F, minimum.y + height * 0.5F };
		if (maximum.x - minimum.x >= height)
			api.DrawTriangleFilled({ center.x - radius, center.y - radius * 0.5F }, { center.x + radius, center.y - radius * 0.5F }, { center.x, center.y + radius }, WidgetColor(ImGuiCol_Text));
	}

	void Icon(std::string_view a_id)
	{
		auto& api = *FUCK::GetInterface();
		const auto [minimum, maximum] = Bounds();
		const ImVec2 center{ (minimum.x + maximum.x) * 0.5F, (minimum.y + maximum.y) * 0.5F };
		const auto   radius = (std::min)(maximum.x - minimum.x, maximum.y - minimum.y) * 0.25F;
		const auto   thickness = (std::max)(1.0F, radius * 0.22F);
		const auto   color = WidgetColor(ImGuiCol_Text);
		if (a_id.starts_with("clear-")) {
			api.DrawLine({ center.x - radius, center.y - radius }, { center.x + radius, center.y + radius }, color, thickness);
			api.DrawLine({ center.x - radius, center.y + radius }, { center.x + radius, center.y - radius }, color, thickness);
			return;
		}
		// Preserve the original glyph's aspect ratio; never stretch it to a square.
		auto& image = ResetImage();
		if (!imageAttempted) {
			imageAttempted = true;
			image = FUCK::Image("Data/Interface/MCMBridge/reset.png");
			if (!image.IsLoaded())
				SKSE::log::warn("FLICK reset icon could not be loaded: Data/Interface/MCMBridge/reset.png");
		}
		if (!image.IsLoaded() || image.GetWidth() <= 0 || image.GetHeight() <= 0)
			return;
		const auto   size = (std::min)(maximum.x - minimum.x, maximum.y - minimum.y) * 0.65F;
		const auto   scale = size / (std::max)(image.GetWidth(), image.GetHeight());
		const ImVec2 extent{ image.GetWidth() * scale, image.GetHeight() * scale };
		api.AddImage(reinterpret_cast<void*>(image.GetID()),
			{ center.x - extent.x * 0.5F, center.y - extent.y * 0.5F },
			{ center.x + extent.x * 0.5F, center.y + extent.y * 0.5F }, { 0, 0 }, { 1, 1 }, color);
	}

	void ReleaseIcon()
	{
		ResetImage().Reset();
		imageAttempted = false;
	}
}
