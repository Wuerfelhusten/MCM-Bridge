#pragma once

#include "MCMBridge/Core/NavigationMerge.h"
#include "MCMBridge/Papyrus/IMCMHostAdapter.h"

namespace MCMBridge
{
	inline bool ReadRegisteredNavigation(const IMCMHostAdapter& a_adapter, MCMMod& a_mod)
	{
		const bool first = a_mod.pages.empty();
		const auto names = a_adapter.ReadRegisteredPages();
		if (!names) {
			// Keep confirmed navigation on a failed read. New registrations still
			// need an entry through which their normal opening callback can run.
			if (first) {
				MergeNavigation(a_mod, {});
				a_mod.pages.front().openingPlaceholder = true;
			}
			return false;
		}
		MergeNavigation(a_mod, *names);
		if (first && a_mod.pages.size() == 1 && a_mod.pages.front().index == -1)
			a_mod.pages.front().openingPlaceholder = true;
		return true;
	}
}
