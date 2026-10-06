#include "MCMBridge/Plugin/BridgeController.h"
#include "MCMBridge/Plugin/WritePauseService.h"

namespace MCMBridge
{
	bool BridgeController::TryFrontendHandoff(std::function<void()> a_completion)
	{
		// Game task only. Never interrupt an admitted VM call or external lease.
		if (refreshing || activeScan || activeWrite || activeHelp || activeHostedPage || activeHostedClose ||
			directContext.id || scriptContext.lease || ExternalOperationBlocked() || WritePauseService::GetSingleton().Pending())
			return false;
		std::uint64_t lease{};
		{
			const std::scoped_lock lock(externalOperationMutex);
			if (externalOperation.Begin("MCMBridge frontend", false, false, lease) != ExternalAdmission::kReady)
				return false;
		}
		CloseFrameworkView();
		const auto operationSession = session;
		CloseHostedSession([this, operationSession, lease, completion = std::move(a_completion)] {
			// Queue even an already-closed session so rendering admission closes
			// before frontend registrations change.
			if (auto* tasks = SKSE::GetTaskInterface())
				tasks->AddTask([this, operationSession, lease, completion] {
					if (operationSession == session) {
						completion();
						EndExternalOperation(lease);
					}
				});
		});
		return true;
	}
}
