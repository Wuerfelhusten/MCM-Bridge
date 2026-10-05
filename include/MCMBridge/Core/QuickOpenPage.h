#pragma once

#include "MCMBridge/Core/Model.h"
#include "MCMBridge/Core/Result.h"

namespace MCMBridge
{
	inline Result<std::string> ResolveQuickOpenPage(const MCMMod& a_mod, std::string_view a_rawPage)
	{
		const MCMPage* selected{};
		for (const auto& page : a_mod.pages) {
			if (page.rawName != a_rawPage)
				continue;
			if (selected)
				return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "The requested page name is ambiguous" });
			selected = &page;
		}
		if (!selected && a_rawPage.empty() && !a_mod.pages.empty())
			selected = &a_mod.pages.front();
		if (!selected)
			return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "The requested page is not present in current navigation" });
		return selected->stableID;
	}
}
