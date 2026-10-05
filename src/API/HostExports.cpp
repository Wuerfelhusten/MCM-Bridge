#include "MCMBridge/API/HostEvents.h"
#include "MCMBridge/API/MCMBridgeHost.h"
#include "MCMBridge/Plugin/BridgeController.h"

#include <unordered_map>

struct MCMHostData
{
	explicit MCMHostData(MCMBridge::HostDataInput a_input) : data(std::move(a_input)) {}
	MCMBridge::HostData data;
};

namespace
{
	std::unordered_map<MCMHostData*, std::unique_ptr<MCMHostData>> dataHandles;
	uint32_t MCM_HOST_CALL                                         Ready()
	{
		const auto& controller = MCMBridge::BridgeController::GetSingleton();
		return controller.IsSessionReady() ? 1U : 0U;
	}
	MCMHostResult MCM_HOST_CALL Begin(const char* a_owner, uint32_t a_restore, MCMHostContext* a_context)
	{
		if (a_context)
			*a_context = 0;
		if (!a_owner || !*a_owner || !a_context || a_restore > 1)
			return MCM_HOST_INVALID_ARGUMENT;
		try {
			return MCMBridge::BridgeController::GetSingleton().BeginHostContext(a_owner, a_restore != 0, *a_context);
		} catch (...) {
			return MCM_HOST_UNAVAILABLE;
		}
	}
	MCMHostResult MCM_HOST_CALL Invoke(MCMHostContext a_context, const MCMHostCall* a_call, MCMHostCompletion a_completion, void* a_user)
	{
		if (!a_call || !a_completion)
			return MCM_HOST_INVALID_ARGUMENT;
		try {
			return MCMBridge::BridgeController::GetSingleton().InvokeHost(a_context, *a_call, a_completion, a_user);
		} catch (...) {
			return MCM_HOST_UNAVAILABLE;
		}
	}
	MCMHostResult MCM_HOST_CALL Cancel(MCMHostContext a_context)
	{
		try {
			return MCMBridge::BridgeController::GetSingleton().EndHostContext(a_context, true);
		} catch (...) {
			return MCM_HOST_UNAVAILABLE;
		}
	}
	MCMHostResult MCM_HOST_CALL End(MCMHostContext a_context)
	{
		try {
			return MCMBridge::BridgeController::GetSingleton().EndHostContext(a_context, false);
		} catch (...) {
			return MCM_HOST_UNAVAILABLE;
		}
	}
	MCMHostResult MCM_HOST_CALL Subscribe(MCMHostObserver a_observer, void* a_user, uint64_t* a_subscription)
	{
		if (!a_subscription)
			return MCM_HOST_INVALID_ARGUMENT;
		*a_subscription = 0;
		try {
			return MCMBridge::HostEvents::GetSingleton().Subscribe(a_observer, a_user, *a_subscription);
		} catch (...) {
			return MCM_HOST_UNAVAILABLE;
		}
	}
	MCMHostResult MCM_HOST_CALL Unsubscribe(uint64_t a_subscription)
	{
		return MCMBridge::HostEvents::GetSingleton().Unsubscribe(a_subscription);
	}
	MCMHostResult MCM_HOST_CALL AcquireData(MCMHostContext a_context, MCMHostData** a_data, MCMHostDataView* a_view)
	{
		if (a_data)
			*a_data = nullptr;
		if (a_view)
			*a_view = {};
		if (!a_data || !a_view)
			return MCM_HOST_INVALID_ARGUMENT;
		try {
			auto input = MCMBridge::BridgeController::GetSingleton().ReadHostData(a_context);
			if (!input)
				return input.error().code == MCMBridge::BridgeErrorCode::kBusy ? MCM_HOST_BUSY : MCM_HOST_UNAVAILABLE;
			auto       data = std::make_unique<MCMHostData>(std::move(*input));
			const auto handle = data.get();
			const auto view = data->data.View();
			dataHandles.emplace(handle, std::move(data));
			*a_data = handle;
			*a_view = view;
			return MCM_HOST_OK;
		} catch (...) {
			return MCM_HOST_UNAVAILABLE;
		}
	}
	void MCM_HOST_CALL ReleaseData(MCMHostData* a_data)
	{
		dataHandles.erase(a_data);
	}
	const MCMBridgeHost host{ Ready, Begin, Invoke, Cancel, End, Subscribe, Unsubscribe, AcquireData, ReleaseData };
}

extern "C" __declspec(dllexport) const MCMBridgeHost* MCM_HOST_CALL MCMBridge_GetHost()
{
	return &host;
}
