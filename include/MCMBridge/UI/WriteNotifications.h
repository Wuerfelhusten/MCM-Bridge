#pragma once

#include <string>

namespace MCMBridge::WriteNotifications
{
	bool Install();
	void Show(std::string a_message);
	void Reset();
}
