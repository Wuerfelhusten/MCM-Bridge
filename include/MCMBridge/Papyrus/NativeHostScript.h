#pragma once

#include "MCMBridge/Papyrus/IClassicScript.h"
#include "RE/Skyrim.h"

namespace MCMBridge
{
	// Admission and session reset belong to the host controller, not this factory.
	Result<std::shared_ptr<IClassicScript>> CreateNativeHostScript(std::uint64_t a_session,
		std::string a_modID, RE::BSTSmartPointer<RE::BSScript::Object> a_script);
}
