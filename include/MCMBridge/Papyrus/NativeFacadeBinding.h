#pragma once

#include "MCMBridge/Papyrus/NativeFacadeFunction.h"
#include "RE/Skyrim.h"

namespace MCMBridge
{
	class NativeFacadeBinding
	{
	public:
		explicit NativeFacadeBinding(RE::BSScript::IVirtualMachine& a_vm) : vm(a_vm) {}

		template <class Function>
		void RegisterFunction(std::string_view a_name, Function a_function)
		{
			RE::BSTSmartPointer<RE::BSScript::IFunction> function{ new NativeFacadeFunction(a_name, a_function) };
			if (!vm.BindNativeMethod(function.get())) {
				valid = false;
				SKSE::log::critical("Could not bind MCMBridgeNative.{}", a_name);
				return;
			}
			vm.SetCallableFromTasklets("MCMBridgeNative", a_name.data(), true);
		}

		bool IsValid() const { return valid; }

	private:
		RE::BSScript::IVirtualMachine& vm;
		bool                           valid{ true };
	};
}
