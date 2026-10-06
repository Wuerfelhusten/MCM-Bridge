#include "MCMBridge/UI/WriteNotifications.h"
#include "MCMBridge/Framework/FrontendWindow.h"

#include "MCMBridge/UI/FrontendUI.h"

#include <deque>
#include <mutex>

namespace
{
	std::mutex                 mutex;
	std::deque<std::string>    messages;
	MCMBridge::FrontendWindow* window{};

	void __stdcall Render()
	{
		const std::scoped_lock lock(mutex);
		if (messages.empty()) {
			window->SetOpen(false);
			return;
		}
		BridgeUI::SetNextWindowSize({ 620.0F, 0.0F }, BridgeUI::ImGuiCond_FirstUseEver);
		bool open = true;
		bool dismiss{};
		if (BridgeUI::Begin("MCM Bridge - Setting change", &open, BridgeUI::ImGuiWindowFlags_AlwaysAutoResize)) {
			BridgeUI::PushTextWrapPos(BridgeUI::GetFontSize() * 32.0F);
			BridgeUI::TextWrapped("%s", messages.front().c_str());
			BridgeUI::PopTextWrapPos();
			BridgeUI::Spacing();
			dismiss = BridgeUI::Button("Dismiss");
			BridgeUI::TextDisabled("Open your MCM frontend to interact with this message.");
		}
		BridgeUI::End();
		if (dismiss || !open)
			messages.pop_front();
		window->SetOpen(!messages.empty());
	}
}

namespace MCMBridge::WriteNotifications
{
	bool Install()
	{
		const std::scoped_lock lock(mutex);
		if (!window)
			window = FrontendWindow::Create("Notifications", "MCM Bridge - Setting change", Render, false, 620, 0, false, Reset);
		return window != nullptr;
	}

	void Show(std::string a_message)
	{
		const std::scoped_lock lock(mutex);
		messages.push_back(std::move(a_message));
		if (window)
			window->SetOpen(true);
	}

	void Reset()
	{
		const std::scoped_lock lock(mutex);
		messages.clear();
		if (window)
			window->SetOpen(false);
	}
}
