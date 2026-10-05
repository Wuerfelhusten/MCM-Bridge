#pragma once

#include "MCMBridge/Core/Model.h"

namespace MCMBridge::SnapshotLocalizer
{
	std::string LocalizeText(std::string a_text);
	void        Localize(MCMMod& a_mod);
}
