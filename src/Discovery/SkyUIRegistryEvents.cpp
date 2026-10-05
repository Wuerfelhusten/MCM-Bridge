#include "MCMBridge/Discovery/SkyUIRegistryEvents.h"

#include "MCMBridge/Plugin/BridgeController.h"

namespace
{
	constexpr std::string_view readyEvent = "SKICP_configManagerReady";
	constexpr std::string_view resetEvent = "SKICP_configManagerReset";
}

namespace MCMBridge
{
	SkyUIRegistryEvents& SkyUIRegistryEvents::GetSingleton()
	{
		static SkyUIRegistryEvents singleton;
		return singleton;
	}

	bool SkyUIRegistryEvents::Install()
	{
		if (installed) {
			return true;
		}
		auto* source = SKSE::GetModCallbackEventSource();
		if (!source) {
			SKSE::log::error("SkyUI registry event source is unavailable");
			return false;
		}
		source->AddEventSink(this);
		installed = true;
		SKSE::log::info("Installed SkyUI registry event listener");
		return true;
	}

	RE::BSEventNotifyControl SkyUIRegistryEvents::ProcessEvent(
		const SKSE::ModCallbackEvent* a_event,
		RE::BSTEventSource<SKSE::ModCallbackEvent>*)
	{
		if (!a_event) {
			return RE::BSEventNotifyControl::kContinue;
		}
		const std::string_view name = a_event->eventName.c_str();
		if (name == readyEvent || name == resetEvent)
			BridgeController::GetSingleton().NotifyRegistryEvent();
		return RE::BSEventNotifyControl::kContinue;
	}
}
