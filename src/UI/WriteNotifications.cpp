#include "MCMBridge/UI/WriteNotifications.h"

#include "SKSEMenuFramework.h"

#include <deque>
#include <mutex>

namespace
{
	std::mutex                                 mutex;
	std::deque<std::string>                    messages;
	SKSEMenuFramework::Model::WindowInterface* window{};

	void __stdcall Render()
	{
		const std::scoped_lock lock(mutex);
		if (messages.empty()) {
			window->IsOpen.store(false);
			return;
		}
		ImGuiMCP::SetNextWindowSize({ 620.0F, 0.0F }, ImGuiMCP::ImGuiCond_FirstUseEver);
		bool open = true;
		bool dismiss{};
		if (ImGuiMCP::Begin("MCM Bridge - Setting change", &open, ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize)) {
			ImGuiMCP::PushTextWrapPos(ImGuiMCP::GetFontSize() * 32.0F);
			ImGuiMCP::TextWrapped("%s", messages.front().c_str());
			ImGuiMCP::PopTextWrapPos();
			ImGuiMCP::Spacing();
			dismiss = ImGuiMCP::Button("Dismiss");
			ImGuiMCP::TextDisabled("Open Menu Framework to interact with this message.");
		}
		ImGuiMCP::End();
		if (dismiss || !open)
			messages.pop_front();
		window->IsOpen.store(!messages.empty());
	}
}

namespace MCMBridge::WriteNotifications
{
	bool Install()
	{
		const std::scoped_lock lock(mutex);
		if (!window)
			window = SKSEMenuFramework::AddWindow(Render, false);
		return window != nullptr;
	}

	void Show(std::string a_message)
	{
		const std::scoped_lock lock(mutex);
		messages.push_back(std::move(a_message));
		if (window)
			window->IsOpen.store(true);
	}

	void Reset()
	{
		const std::scoped_lock lock(mutex);
		messages.clear();
		if (window)
			window->IsOpen.store(false);
	}
}
