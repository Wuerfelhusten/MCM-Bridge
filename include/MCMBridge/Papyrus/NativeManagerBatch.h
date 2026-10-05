#pragma once

#include "RE/Skyrim.h"

namespace MCMBridge
{
	// Game-queue only. Non-registration operations flush earlier requests first.
	void QueueNativeRegistration(RE::BSTSmartPointer<RE::BSScript::Object> a_manager,
		RE::BSTSmartPointer<RE::BSScript::Object> a_menu, std::string a_name,
		std::int32_t a_request, std::uint64_t a_session);
	void FlushNativeRegistrations();
}
