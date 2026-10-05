#include "MCMBridge/Papyrus/NativeHostProtocol.h"

#include "MCMBridge/Core/ScopedFrontendRedraw.h"
#include "MCMBridge/Papyrus/NativeHostCallback.h"
#include "MCMBridge/Papyrus/NativeHostUI.h"

namespace
{
	using NativeBase = RE::BSScript::NF_util::NativeFunctionBase;

	template <class T>
	class CapturedInvoke final : public RE::NativeFunction<void(RE::StaticFunctionTag*, RE::BSFixedString, RE::BSFixedString, T)>
	{
		using Base = RE::NativeFunction<void(RE::StaticFunctionTag*, RE::BSFixedString, RE::BSFixedString, T)>;

	public:
		CapturedInvoke(std::string_view a_name, RE::BSTSmartPointer<RE::BSScript::IFunction> a_original) :
			Base(a_name, "UI", [](RE::StaticFunctionTag*, RE::BSFixedString, RE::BSFixedString, T) {}), original(std::move(a_original))
		{
			this->SetCallableFromTasklets(original->CanBeCalledFromTasklets());
		}

		bool MarshallAndDispatch(RE::BSScript::Variable& a_base, RE::BSScript::Internal::VirtualMachine& a_vm,
			RE::VMStackID a_stackID, RE::BSScript::Variable& a_result, const RE::BSScript::StackFrame& a_frame) const override
		{
			bool authenticated = false;
			if constexpr (std::is_same_v<T, std::vector<std::int32_t>> || std::is_same_v<T, bool>) {
				const auto page = a_frame.GetPageForFrame();
				const auto menu = a_frame.GetStackFrameVariable(0, page).Unpack<RE::BSFixedString>();
				const auto target = a_frame.GetStackFrameVariable(1, page).Unpack<RE::BSFixedString>();
				if constexpr (std::is_same_v<T, bool>) {
					if (MCMBridge::NativeHostUI::Close(a_frame, menu.c_str(), target.c_str(), a_frame.GetStackFrameVariable(2, page).Unpack<bool>()))
						return true;
					if (MCMBridge::NativeHostUI::Unlock(a_frame, menu.c_str(), target.c_str()))
						return true;
				} else {
					const auto values = a_frame.GetStackFrameVariable(2, page).Unpack<T>();
					if (MCMBridge::NativeHostUI::SelectPage(a_frame, menu.c_str(), target.c_str(), values))
						return true;
				}
			}
			if constexpr (std::is_same_v<T, std::vector<RE::BSFixedString>>) {
				const auto page = a_frame.GetPageForFrame();
				const auto menu = a_frame.GetStackFrameVariable(0, page).Unpack<RE::BSFixedString>();
				const auto target = a_frame.GetStackFrameVariable(1, page).Unpack<RE::BSFixedString>();
				const auto pages = a_frame.GetStackFrameVariable(2, page).Unpack<T>();
				if (MCMBridge::NativeHostUI::SetNavigation(a_frame, menu.c_str(), target.c_str(), pages))
					return true;
			}
			if constexpr (std::is_same_v<T, std::int32_t>) {
				if (this->GetName() == "InvokeInt") {
					const auto page = a_frame.GetPageForFrame();
					const auto menu = a_frame.GetStackFrameVariable(0, page).Unpack<RE::BSFixedString>();
					const auto target = a_frame.GetStackFrameVariable(1, page).Unpack<RE::BSFixedString>();
					if (MCMBridge::NativeHostUI::Focus(a_frame, menu.c_str(), target.c_str()))
						return true;
				}
				if (this->GetName() == "SetInt") {
					const auto page = a_frame.GetPageForFrame();
					const auto menu = a_frame.GetStackFrameVariable(0, page).Unpack<RE::BSFixedString>();
					const auto target = a_frame.GetStackFrameVariable(1, page).Unpack<RE::BSFixedString>();
					const auto slot = a_frame.GetStackFrameVariable(2, page).Unpack<std::int32_t>();
					if (MCMBridge::NativeHostUI::SetCursor(a_frame, menu.c_str(), target.c_str(), slot))
						return true;
				}
			}
			RE::BSFixedString ownedMenu("");
			RE::BSFixedString ownedTarget("");
			if (a_frame.parent) {
				const auto owner = a_frame.parent->callback;
				if (auto* callback = MCMBridge::NativeHostCallback::Find(owner.get())) {
					const auto page = a_frame.GetPageForFrame();
					const auto menu = a_frame.GetStackFrameVariable(0, page).Unpack<RE::BSFixedString>();
					const auto target = a_frame.GetStackFrameVariable(1, page).Unpack<RE::BSFixedString>();
					ownedMenu = menu;
					ownedTarget = target;
					const auto value = a_frame.GetStackFrameVariable(2, page).Unpack<T>();
					const auto kind = std::string_view(this->GetName().c_str()).starts_with("Set") ?
					                      MCMBridge::HostProtocolCallKind::kSet :
					                      MCMBridge::HostProtocolCallKind::kInvoke;
					if constexpr (std::is_same_v<T, std::vector<RE::BSFixedString>>) {
						std::vector<std::string> strings;
						strings.reserve(value.size());
						for (const auto& item : value) strings.emplace_back(item.c_str());
						callback->Observe(a_frame, menu.c_str(), target.c_str(), std::move(strings), kind);
					} else if constexpr (std::is_same_v<T, RE::BSFixedString>) {
						callback->Observe(a_frame, menu.c_str(), target.c_str(), std::string(value.c_str()), kind);
					} else {
						const auto observed = callback->Observe(a_frame, menu.c_str(), target.c_str(), value, kind);
						if constexpr (std::is_same_v<T, bool>)
							authenticated = observed && kind == MCMBridge::HostProtocolCallKind::kInvoke;
					}
				}
			}
			const MCMBridge::ScopedFrontendRedraw redraw(authenticated, ownedMenu.c_str(), ownedTarget.c_str());
			return static_cast<NativeBase*>(original.get())->MarshallAndDispatch(a_base, a_vm, a_stackID, a_result, a_frame);
		}

	private:
		RE::BSTSmartPointer<RE::BSScript::IFunction> original;
	};

	RE::BSTSmartPointer<RE::BSScript::IFunction> Find(RE::BSScript::ObjectTypeInfo& a_type, std::string_view a_name)
	{
		if (a_type.IsLinked()) {
			const auto* functions = a_type.GetGlobalFuncIter();
			for (std::uint32_t index = 0; index < a_type.GetNumGlobalFuncs(); ++index)
				if (functions[index].func && functions[index].func->GetName() == a_name)
					return functions[index].func;
		} else {
			for (const auto* entry = a_type.GetUnlinkedFunctionIter(); entry; entry = entry->next)
				if (entry->func && entry->func->GetName() == a_name)
					return entry->func;
		}
		return {};
	}

	template <class T>
	bool Bind(RE::BSScript::IVirtualMachine& a_vm, RE::BSScript::ObjectTypeInfo& a_type, std::string_view a_name)
	{
		auto original = Find(a_type, a_name);
		if (!original || !original->GetIsNative() || original->GetParamCount() != 3)
			return false;
		// SKSE declares this ABI without CommonLib's C++ namespaces. Cross-module
		// dynamic_cast would reject valid SKSE natives despite the shared VM ABI.
		if (!original->GetIsStatic() || static_cast<NativeBase*>(original.get())->GetIsLatent())
			return false;
		RE::BSTSmartPointer<RE::BSScript::IFunction> replacement{ new CapturedInvoke<T>(a_name, original) };
		if (replacement->GetReturnType() != original->GetReturnType())
			return false;
		for (std::uint32_t index = 0; index < 3; ++index) {
			RE::BSFixedString      name;
			RE::BSScript::TypeInfo expected;
			RE::BSScript::TypeInfo actual;
			replacement->GetParam(index, name, expected);
			original->GetParam(index, name, actual);
			if (expected != actual)
				return false;
		}
		return a_vm.BindNativeMethod(replacement.get());
	}
}

namespace MCMBridge::NativeHostProtocol
{
	bool Register(RE::BSScript::IVirtualMachine* a_virtualMachine)
	{
		RE::BSTSmartPointer<RE::BSScript::ObjectTypeInfo> ui;
		if (!a_virtualMachine || !a_virtualMachine->GetScriptObjectType(RE::BSFixedString("UI"), ui) || !ui)
			return false;
		const auto integers = Bind<std::vector<std::int32_t>>(*a_virtualMachine, *ui, "InvokeIntA");
		const auto numbers = Bind<std::vector<float>>(*a_virtualMachine, *ui, "InvokeFloatA");
		const auto strings = Bind<std::vector<RE::BSFixedString>>(*a_virtualMachine, *ui, "InvokeStringA");
		const auto flush = Bind<std::int32_t>(*a_virtualMachine, *ui, "InvokeInt");
		const auto cursor = Bind<std::int32_t>(*a_virtualMachine, *ui, "SetInt");
		const auto scalar = Bind<float>(*a_virtualMachine, *ui, "SetFloat");
		const auto text = Bind<RE::BSFixedString>(*a_virtualMachine, *ui, "SetString");
		const auto invalidate = Bind<bool>(*a_virtualMachine, *ui, "InvokeBool");
		const auto stringInvoke = Bind<RE::BSFixedString>(*a_virtualMachine, *ui, "InvokeString");
		const auto complete = integers && numbers && strings && flush;
		SKSE::log::info("Native host protocol observation: int_arrays={} float_arrays={} string_arrays={} flush={}", integers, numbers, strings, flush);
		SKSE::log::info("Native host control observation: cursor={} numeric_value={} string_value={} invalidation={} string_invoke={}", cursor, scalar, text, invalidate, stringInvoke);
		const auto reads = NativeHostUI::Register(*a_virtualMachine, *ui, cursor && strings && integers && invalidate);
		return complete && cursor && scalar && text && invalidate && stringInvoke && reads;
	}
}
