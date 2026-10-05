#include "MCMBridge/Plugin/BridgeController.h"

namespace MCMBridge
{
	Result<bool> BridgeController::ResetNativeHost(const RE::BSTSmartPointer<RE::BSScript::Object>& a_manager)
	{
		if (!sessionReady.load() || !registry.Native().Owns(a_manager))
			return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "Native manager does not own the current registry" });
		if (ExternalOperationBlocked() || activeScan || activeWrite || activeHelp || HasHostedSession())
			return std::unexpected(BridgeError{ BridgeErrorCode::kBusy, "Native registry reset cannot interrupt an MCM operation" });
		auto cleared = registry.Native().Clear();
		if (!cleared)
			return cleared;
		RequestRefresh(true);
		return true;
	}

	Result<bool> BridgeController::ActivateNativeHost(RE::BSTSmartPointer<RE::BSScript::Object> a_manager)
	{
		auto*                                     vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		RE::BSTSmartPointer<RE::BSScript::Object> live;
		if (!vm || !a_manager || !a_manager->GetTypeInfo() ||
			!vm->FindBoundObject(a_manager->GetHandle(), a_manager->GetTypeInfo()->GetName(), live) || live != a_manager)
			return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "Native manager instance is no longer bound" });
		if (!sessionReady.load())
			return std::unexpected(BridgeError{ BridgeErrorCode::kBusy, "Native host is waiting for the loaded game" });
		if (registry.Native().Owns(a_manager))
			return true;
		if (ExternalOperationBlocked() || activeScan || activeWrite || activeHelp || HasHostedSession())
			return std::unexpected(BridgeError{ BridgeErrorCode::kBusy, "Native host bootstrap cannot interrupt an MCM operation" });
		const auto* version = a_manager ? a_manager->GetProperty("MCMBridgeManagerVersion") : nullptr;
		if (!version || !version->IsInt() || version->GetSInt() != 1)
			return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "Native manager facade is missing or incompatible" });
		auto activated = registry.ActivateNative(std::move(a_manager), session);
		if (!activated)
			return std::unexpected(activated.error());
		registryIDs.clear();
		RequestRefresh(true);
		SKSE::log::info("Native MCM host registry activated for session {}", session);
		return true;
	}
}
