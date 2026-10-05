#pragma once

#include "MCMBridge/Papyrus/NativeFacade.h"
#include "MCMBridge/Papyrus/NativeHostUI.h"

namespace MCMBridge
{
	template <class Function>
	using FacadeFunctionBase = decltype(RE::NativeFunction(std::string_view{}, std::string_view{}, std::declval<Function>()));

	template <class Function>
	class NativeFacadeFunction final : public FacadeFunctionBase<Function>
	{
		using Base = FacadeFunctionBase<Function>;

	public:
		NativeFacadeFunction(std::string_view a_name, Function a_function) :
			Base(a_name, "MCMBridgeNative", a_function), guarded(a_name != "GetProtocolVersion" && a_name != "LogError"),
			failure(a_name == "AddOption" ? -1 : a_name == "TakeMessage" ? -2 :
																		   0) {}

		bool MarshallAndDispatch(RE::BSScript::Variable& a_base, RE::BSScript::Internal::VirtualMachine& a_vm,
			RE::VMStackID a_stack, RE::BSScript::Variable& a_result, const RE::BSScript::StackFrame& a_frame) const override
		{
			// An old stack may read a newly rebound script token. Its original
			// callback owner, not that mutable script field, decides expiry.
			if (guarded) {
				const auto owner = NativeHostUI::ResolveCallbackSession(a_frame);
				if (owner && !NativeFacadeSession().IsActive(*owner)) {
					if constexpr (std::is_same_v<typename Base::result_type, bool>)
						a_result.SetBool(false);
					else if constexpr (std::is_same_v<typename Base::result_type, std::int32_t>)
						a_result.SetSInt(failure);
					else
						a_result.SetNone();
					return true;
				}
			}
			return Base::MarshallAndDispatch(a_base, a_vm, a_stack, a_result, a_frame);
		}

	private:
		bool         guarded;
		std::int32_t failure;
	};
}
