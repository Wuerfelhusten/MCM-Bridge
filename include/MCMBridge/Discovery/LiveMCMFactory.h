#pragma once

#include "MCMBridge/Discovery/LiveMCM.h"
#include "RE/Skyrim.h"

namespace MCMBridge
{
	Result<LiveMCM> CreateLiveMCM(
		RE::BSTSmartPointer<RE::BSScript::Object> a_script);
}
