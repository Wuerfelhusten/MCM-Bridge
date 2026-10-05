#pragma once

#include <cstdint>
#include <string>

namespace MCMBridge::QuickOpenWindow
{
	bool Install();
	void Open(std::uint64_t a_session, std::string a_modID, std::string a_page);
	void Close();
}
