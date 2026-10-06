#pragma once

#include <atomic>
#include <functional>
#include <string>
#include <vector>

namespace MCMBridge
{
	class FrontendWindow
	{
	public:
		using Renderer = void(__stdcall*)();
		static FrontendWindow* Create(std::string a_id, std::string a_title, Renderer a_render, bool a_blocking,
			float a_width, float a_height, bool a_primary = false, std::function<void()> a_closed = {});
		static void            CloseAll();
		static void            RefreshBackend();
		static bool            PrimaryOpen();
		void                   SetOpen(bool a_open);
		void                   UserClose();
		void                   Render();
		bool                   IsOpen() const { return open.load(); }
		std::string            id;
		std::string            title;
		bool                   blocking{};
		bool                   primary{};
		float                  width{};
		float                  height{};

	private:
		std::atomic_bool      open{};
		Renderer              renderer{};
		std::function<void()> closed;
		void*                 frameworkWindow{};
	};
}
