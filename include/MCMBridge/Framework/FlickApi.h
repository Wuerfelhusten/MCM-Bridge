#pragma once

#include "MCMBridge/Core/Model.h"

#include <span>

namespace MCMBridge
{
	class FrontendWindow;
}
namespace MCMBridge::FlickApi
{
	void RegisterWindow(FrontendWindow& a_window);
	bool Bind();
	void Install();
	void Synchronize(std::span<const MCMMod> a_mods);
	void RenderPages(const MCMMod& a_mod, std::string& a_page);
	void SetOpen(bool a_open);
	bool IsOpen();
}
