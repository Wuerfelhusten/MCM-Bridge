#pragma once

#include <functional>
#include <string>
#include <vector>

namespace MCMBridge::MessageDialog
{
	bool Install();
	// Completion runs on the game queue. Validity may be read from the render thread.
	bool Handle(std::vector<std::string> a_arguments, std::function<void(bool)> a_completion = {}, std::function<bool()> a_valid = {});
	void Cancel();
}
