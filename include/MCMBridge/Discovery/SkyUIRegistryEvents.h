#pragma once

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

namespace MCMBridge
{
	class SkyUIRegistryEvents final : public RE::BSTEventSink<SKSE::ModCallbackEvent>
	{
	public:
		static SkyUIRegistryEvents& GetSingleton();
		bool                        Install();

		RE::BSEventNotifyControl ProcessEvent(
			const SKSE::ModCallbackEvent*               a_event,
			RE::BSTEventSource<SKSE::ModCallbackEvent>* a_source) override;

	private:
		bool installed{};
	};
}
