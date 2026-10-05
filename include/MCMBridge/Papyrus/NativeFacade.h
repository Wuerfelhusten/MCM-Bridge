#pragma once

#include "MCMBridge/Core/NativeHostSession.h"
#include "MCMBridge/Core/NativeRegistrationRequests.h"
#include "RE/Skyrim.h"

#include <string>

namespace MCMBridge
{
	// Only the game-thread controller may open, reset or close the execution owner.
	NativeHostSession&          NativeFacadeSession();
	NativeRegistrationRequests& NativeRegistryRequests();
	NativeRegistrationRequests& NativeCallRequests();
	bool                        RegisterNativeCallAdmission(RE::BSScript::IVirtualMachine& a_vm);
	bool                        RegisterNativeFacade(RE::BSScript::IVirtualMachine* a_vm);
	bool                        RegisterNativePageAdmission(RE::BSScript::IVirtualMachine& a_vm);
	bool                        RegisterNativeFacadeRuntime(RE::BSScript::IVirtualMachine* a_vm);
	bool                        IsNativeFacadeReady();
	bool                        HasNativeFacadeContract(const RE::BSTSmartPointer<RE::BSScript::Object>& a_script, bool a_manager = false, std::string* a_reason = nullptr);
	bool                        RegisterNativeManager(RE::BSScript::IVirtualMachine* a_vm);
}
