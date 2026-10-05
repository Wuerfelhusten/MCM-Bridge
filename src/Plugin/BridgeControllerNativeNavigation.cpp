#include "MCMBridge/Plugin/BridgeController.h"

#include "MCMBridge/Papyrus/RegisteredNavigation.h"

namespace MCMBridge
{
	void BridgeController::ReadNativeNavigation()
	{
		const auto  started = std::chrono::steady_clock::now();
		std::size_t unavailable{};
		for (std::size_t index = 0; index < liveEntries.size(); ++index) {
			const auto& live = liveEntries[index];
			if (quarantined.contains(live.descriptor.stableID))
				continue;
			auto& mod = pendingSnapshot.mods[index];
			// Registration of another MCM must not restart a confirmed user session.
			// Its own navigation remains owned by callbacks and visible-page polling.
			if (hostedScript && hostedReady && mod.stableID == hostedDescriptor.stableID &&
				viewLoad.Ready(mod.stableID, hostedPageID))
				continue;
			if (!live.adapter || !ReadRegisteredNavigation(*live.adapter, mod)) {
				++unavailable;
				pendingSnapshot.diagnostics.push_back({ DiagnosticSeverity::kWarning, mod.stableID,
					"Registered navigation is unavailable. Open the MCM to initialize its pages." });
			}
		}
		const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
		SKSE::log::info("Native registered navigation: mods={} unavailable={} callbacks=0 elapsed_ms={:.3f}",
			liveEntries.size(), unavailable, elapsed);
		// This publishes navigation once, not an empty/intermediate page catalog.
		// Page activation and explicit content scans still execute original callbacks.
		FinishScan(session);
	}
}
