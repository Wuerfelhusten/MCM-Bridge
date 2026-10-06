#pragma once

#include "MCMBridge/Core/FrontendSelection.h"
#include "MCMBridge/Core/SkyUIRichText.h"

namespace MCMBridge
{
	// Per-render-thread routing. No ImGui object crosses frontend boundaries.
	inline thread_local Frontend renderFrontend{ Frontend::kMenuFramework };
	using RichTextDraw = void (*)(const SkyUIRichText&, bool, bool);
	inline thread_local RichTextDraw nativeRichTextDraw{};
	inline thread_local bool         hostedWindowDraw{};

	class RenderContext
	{
	public:
		RenderContext(Frontend a_frontend, RichTextDraw a_draw = nullptr, bool a_window = false) : previous(renderFrontend), previousDraw(nativeRichTextDraw), previousWindow(hostedWindowDraw)
		{
			renderFrontend = a_frontend;
			nativeRichTextDraw = a_draw;
			hostedWindowDraw = a_window;
		}
		~RenderContext()
		{
			renderFrontend = previous;
			nativeRichTextDraw = previousDraw;
			hostedWindowDraw = previousWindow;
		}
		RenderContext(const RenderContext&) = delete;
		RenderContext& operator=(const RenderContext&) = delete;

	private:
		Frontend     previous;
		RichTextDraw previousDraw;
		bool         previousWindow;
	};
}
