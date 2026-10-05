#include "MCMBridge/Papyrus/NativeHostUI.h"

#include "MCMBridge/Core/PapyrusIdentifier.h"
#include "MCMBridge/Papyrus/NativeFacade.h"

namespace
{
	using NativeBase = RE::BSScript::NF_util::NativeFunctionBase;

	class MenuMode final : public RE::NativeFunction<bool(RE::StaticFunctionTag*)>
	{
		using Base = RE::NativeFunction<bool(RE::StaticFunctionTag*)>;

	public:
		explicit MenuMode(RE::BSTSmartPointer<RE::BSScript::IFunction> a_original) :
			Base("IsInMenuMode", "Utility", [](RE::StaticFunctionTag*) { return false; }), original(std::move(a_original))
		{
			SetCallableFromTasklets(original->CanBeCalledFromTasklets());
		}
		bool MarshallAndDispatch(RE::BSScript::Variable& a_base, RE::BSScript::Internal::VirtualMachine& a_vm,
			RE::VMStackID a_stackID, RE::BSScript::Variable& a_result, const RE::BSScript::StackFrame& a_frame) const override
		{
			// Mod events have separate VM stacks. Match their actual self to the
			// active owner, not a mod name or the presence of any framework window.
			const auto* caller = a_frame.previousFrame;
			while (caller && !caller->self.IsObject())
				caller = caller->previousFrame;
			if (caller && caller->self.GetObject() &&
				MCMBridge::NativeFacadeSession().TokenForOwner(reinterpret_cast<std::uintptr_t>(caller->self.GetObject().get()))) {
				a_result.SetBool(true);
				return true;
			}
			return static_cast<NativeBase*>(original.get())->MarshallAndDispatch(a_base, a_vm, a_stackID, a_result, a_frame);
		}

	private:
		RE::BSTSmartPointer<RE::BSScript::IFunction> original;
	};
}

namespace MCMBridge::NativeHostUI
{
	bool RegisterMenuMode(RE::BSScript::IVirtualMachine& a_vm)
	{
		RE::BSTSmartPointer<RE::BSScript::ObjectTypeInfo> type;
		if (!a_vm.GetScriptObjectType("Utility", type) || !type)
			return false;
		RE::BSTSmartPointer<RE::BSScript::IFunction> original;
		if (type->IsLinked()) {
			const auto* functions = type->GetGlobalFuncIter();
			for (std::uint32_t index = 0; index < type->GetNumGlobalFuncs(); ++index)
				if (functions[index].func && SamePapyrusIdentifier(functions[index].func->GetName().c_str(), "IsInMenuMode"))
					original = functions[index].func;
		} else {
			for (const auto* entry = type->GetUnlinkedFunctionIter(); entry; entry = entry->next)
				if (entry->func && SamePapyrusIdentifier(entry->func->GetName().c_str(), "IsInMenuMode"))
					original = entry->func;
		}
		if (!original || !original->GetIsNative() || !original->GetIsStatic() || original->GetParamCount() != 0 ||
			static_cast<NativeBase*>(original.get())->GetIsLatent())
			return false;
		RE::BSTSmartPointer<RE::BSScript::IFunction> replacement{ new MenuMode(original) };
		return replacement->GetReturnType() == original->GetReturnType() && a_vm.BindNativeMethod(replacement.get());
	}
}
