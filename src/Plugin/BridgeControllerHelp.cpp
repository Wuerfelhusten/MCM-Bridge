#include "MCMBridge/Plugin/BridgeController.h"

#include "MCMBridge/Localization/SnapshotLocalizer.h"
#include "MCMBridge/Plugin/GameSessionEvents.h"
#include "MCMBridge/Plugin/TaskScheduler.h"

#include <format>
#include <type_traits>

namespace
{
	constexpr auto retryDelay = std::chrono::milliseconds(50);

	std::string RequestKey(const MCMBridge::SettingIdentity& a_identity, const MCMBridge::MCMValue& a_value)
	{
		return std::visit([&a_identity](const auto& a_current) {
			using T = std::decay_t<decltype(a_current)>;
			if constexpr (std::is_same_v<T, std::monostate>) {
				return std::format("{}:none", a_identity.stableID);
			} else if constexpr (std::is_same_v<T, bool>) {
				return std::format("{}:{}", a_identity.stableID, a_current ? 1 : 0);
			} else {
				return std::format("{}:{}", a_identity.stableID, a_current);
			}
		},
			a_value);
	}
}

namespace MCMBridge
{
	void BridgeController::RequestControlHelp(SettingIdentity a_identity, MCMValue a_value)
	{
		if (a_identity.backend != MCMBackendKind::kClassicSkyUI || a_identity.stableID.empty()) {
			return;
		}
		auto requestKey = std::format("{}:{}", snapshots.Get()->generation, RequestKey(a_identity, a_value));
		{
			const std::scoped_lock lock(helpMutex);
			if (resolvedHelp.contains(requestKey) || !pendingHelp.insert(requestKey).second) {
				return;
			}
		}
		const auto operationSession = session;
		if (auto* tasks = SKSE::GetTaskInterface()) {
			tasks->AddTask([identity = std::move(a_identity), requestKey = std::move(requestKey), operationSession]() mutable {
				GetSingleton().StartControlHelp(std::move(identity), std::move(requestKey), operationSession);
			});
		} else {
			const std::scoped_lock lock(helpMutex);
			pendingHelp.erase(requestKey);
		}
	}

	void BridgeController::StartControlHelp(SettingIdentity a_identity, std::string a_requestKey, std::uint64_t a_operationSession)
	{
		auto release = [this, &a_requestKey] {
			const std::scoped_lock lock(helpMutex);
			pendingHelp.erase(a_requestKey);
		};
		if (a_operationSession != session || !GameSessionEvents::CanUseGame()) {
			release();
			return;
		}
		if (ExternalOperationBlocked()) {
			TaskScheduler::GetSingleton().After(retryDelay, [identity = std::move(a_identity), requestKey = std::move(a_requestKey), a_operationSession]() mutable {
				GetSingleton().StartControlHelp(std::move(identity), std::move(requestKey), a_operationSession);
			});
			return;
		}
		if (refreshing || activeScan || activeWrite || activeHelp || activeHostedPage || activeHostedClose) {
			TaskScheduler::GetSingleton().After(retryDelay, [identity = std::move(a_identity), requestKey = std::move(a_requestKey), a_operationSession]() mutable {
				GetSingleton().StartControlHelp(std::move(identity), std::move(requestKey), a_operationSession);
			});
			return;
		}
		if (!IsHostedTarget(a_identity)) {
			release();
			return;
		}

		const auto settingID = a_identity.stableID;
		activeHelp = std::make_shared<ClassicHelpOperation>(
			hostedScript,
			std::move(a_identity),
			ClassicHelpOperation::BusyCheck{},
			[this, a_operationSession, settingID, requestKey = std::move(a_requestKey)](Result<std::string> a_result) {
				if (a_operationSession != session) {
					return;
				}
				activeHelp.reset();
				{
					const std::scoped_lock lock(helpMutex);
					pendingHelp.erase(requestKey);
					resolvedHelp.insert(requestKey);
				}
				if (a_result && !a_result->empty()) {
					snapshots.SetControlHelp(settingID, SnapshotLocalizer::LocalizeText(std::move(*a_result)));
				}
				ProcessWrites();
				DriveHostedPage();
			},
			TaskScheduler::GetSingleton());
		const auto operation = activeHelp;
		operation->Start();
	}
}
