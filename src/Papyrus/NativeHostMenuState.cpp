#include "MCMBridge/Papyrus/NativeHostUI.h"

#include "MCMBridge/Papyrus/NativeFacade.h"

namespace
{
	using NativeBase = RE::BSScript::NF_util::NativeFunctionBase;

	class MenuState final : public RE::NativeFunction<bool(RE::StaticFunctionTag*, RE::BSFixedString)>
	{
		using Base = RE::NativeFunction<bool(RE::StaticFunctionTag*, RE::BSFixedString)>;

	public:
		explicit MenuState(RE::BSTSmartPointer<RE::BSScript::IFunction> a_original) :
			Base("IsMenuOpen", "UI", [](RE::StaticFunctionTag*, RE::BSFixedString) { return false; }), original(std::move(a_original))
		{
			SetCallableFromTasklets(original->CanBeCalledFromTasklets());
		}
		bool MarshallAndDispatch(RE::BSScript::Variable& a_base, RE::BSScript::Internal::VirtualMachine& a_vm,
			RE::VMStackID a_stackID, RE::BSScript::Variable& a_result, const RE::BSScript::StackFrame& a_frame) const override
		{
			const auto page = a_frame.GetPageForFrame();
			const auto menu = a_frame.GetStackFrameVariable(0, page).Unpack<RE::BSFixedString>();
			if (const auto token = MCMBridge::NativeHostUI::ResolveSession(a_frame, menu.c_str())) {
				a_result.SetBool(MCMBridge::NativeFacadeSession().IsActive(*token));
				return true;
			}
			if (MCMBridge::NativeHostUI::InterceptExternalState(a_frame, menu.c_str(), {}, a_result))
				return true;
			return static_cast<NativeBase*>(original.get())->MarshallAndDispatch(a_base, a_vm, a_stackID, a_result, a_frame);
		}

	private:
		RE::BSTSmartPointer<RE::BSScript::IFunction> original;
	};

	RE::BSTSmartPointer<RE::BSScript::IFunction> Find(RE::BSScript::ObjectTypeInfo& a_type)
	{
		if (a_type.IsLinked()) {
			const auto* functions = a_type.GetGlobalFuncIter();
			for (std::uint32_t index = 0; index < a_type.GetNumGlobalFuncs(); ++index)
				if (functions[index].func && functions[index].func->GetName() == "IsMenuOpen")
					return functions[index].func;
		} else {
			for (const auto* entry = a_type.GetUnlinkedFunctionIter(); entry; entry = entry->next)
				if (entry->func && entry->func->GetName() == "IsMenuOpen")
					return entry->func;
		}
		return {};
	}
}

namespace MCMBridge::NativeHostUI
{
	bool RegisterMenuState(RE::BSScript::IVirtualMachine& a_vm, RE::BSScript::ObjectTypeInfo& a_type)
	{
		auto original = Find(a_type);
		if (!original || !original->GetIsNative() || !original->GetIsStatic() || original->GetParamCount() != 1 ||
			static_cast<NativeBase*>(original.get())->GetIsLatent())
			return false;
		RE::BSTSmartPointer<RE::BSScript::IFunction> replacement{ new MenuState(original) };
		if (replacement->GetReturnType() != original->GetReturnType())
			return false;
		RE::BSFixedString      name;
		RE::BSScript::TypeInfo expected;
		RE::BSScript::TypeInfo actual;
		replacement->GetParam(0, name, expected);
		original->GetParam(0, name, actual);
		return expected == actual && a_vm.BindNativeMethod(replacement.get());
	}
}
