#include "MCMBridge/Core/RegistrySettler.h"

#include <algorithm>

namespace MCMBridge
{
	RegistrySettleResult RegistrySettler::Observe(std::vector<std::string> a_ids)
	{
		++checks;
		std::ranges::sort(a_ids);
		if (a_ids.empty()) {
			quietChecks = 0;
			return checks >= maximumChecks ? RegistrySettleResult::kExpired : RegistrySettleResult::kEmpty;
		}
		if (a_ids != lastIDs) {
			lastIDs = std::move(a_ids);
			quietChecks = 0;
			return RegistrySettleResult::kChanged;
		}

		++quietChecks;
		if (quietChecks >= requiredQuietChecks) {
			return RegistrySettleResult::kReady;
		}
		return checks >= maximumChecks ? RegistrySettleResult::kExpired : RegistrySettleResult::kWaiting;
	}

	void RegistrySettler::Reset()
	{
		lastIDs.clear();
		checks = 0;
		quietChecks = 0;
	}

	bool RegistrySettler::ShouldContinue() const
	{
		return checks < maximumChecks && quietChecks < requiredQuietChecks;
	}
}
