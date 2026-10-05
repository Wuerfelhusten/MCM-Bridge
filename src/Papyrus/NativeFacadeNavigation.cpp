#include "MCMBridge/Papyrus/NativeFacade.h"
#include "MCMBridge/Papyrus/NativeHostUI.h"

namespace
{
	class ConfigAdmission final : public RE::NativeFunction<std::int32_t(RE::StaticFunctionTag*)>
	{
		using Base = RE::NativeFunction<std::int32_t(RE::StaticFunctionTag*)>;

	public:
		ConfigAdmission() : Base("AcquireConfig", "MCMBridgeNative", [](RE::StaticFunctionTag*) { return std::int32_t{}; })
		{
			SetCallableFromTasklets(true);
		}
		bool MarshallAndDispatch(RE::BSScript::Variable&, RE::BSScript::Internal::VirtualMachine&,
			RE::VMStackID, RE::BSScript::Variable& a_result, const RE::BSScript::StackFrame& a_frame) const override
		{
			a_result.SetSInt(0);
			const auto* caller = a_frame.previousFrame;
			if (!caller || !caller->owningFunction || !caller->self.IsObject() ||
				caller->owningFunction->GetObjectTypeName() != "SKI_ConfigBase" || caller->owningFunction->GetName() != "OpenConfig")
				return true;
			const auto script = caller->self.GetObject();
			const auto owned = MCMBridge::NativeHostUI::ResolveCallbackSession(a_frame);
			auto&      host = MCMBridge::NativeFacadeSession();
			if (script && owned && host.IsActive(*owned) &&
				host.TokenForOwner(reinterpret_cast<std::uintptr_t>(script.get())) == *owned)
				a_result.SetSInt(*owned);
			return true;
		}
	};

	class PageAdmission final : public RE::NativeFunction<bool(RE::StaticFunctionTag*, std::int32_t, RE::BSFixedString, std::int32_t)>
	{
		using Base = RE::NativeFunction<bool(RE::StaticFunctionTag*, std::int32_t, RE::BSFixedString, std::int32_t)>;

	public:
		PageAdmission() : Base("AdmitPage", "MCMBridgeNative", [](RE::StaticFunctionTag*, std::int32_t, RE::BSFixedString, std::int32_t) { return false; })
		{
			SetCallableFromTasklets(true);
		}
		bool MarshallAndDispatch(RE::BSScript::Variable&, RE::BSScript::Internal::VirtualMachine&,
			RE::VMStackID, RE::BSScript::Variable& a_result, const RE::BSScript::StackFrame& a_frame) const override
		{
			a_result.SetBool(false);
			const auto* caller = a_frame.previousFrame;
			if (!caller || !caller->owningFunction || !caller->self.IsObject() ||
				caller->owningFunction->GetObjectTypeName() != "SKI_ConfigBase" || caller->owningFunction->GetName() != "SetPage")
				return true;
			const auto  framePage = a_frame.GetPageForFrame();
			const auto& tokenValue = a_frame.GetStackFrameVariable(0, framePage);
			const auto& pageValue = a_frame.GetStackFrameVariable(1, framePage);
			const auto& indexValue = a_frame.GetStackFrameVariable(2, framePage);
			if (!tokenValue.IsInt() || !pageValue.IsString() || !indexValue.IsInt())
				return true;
			auto       script = caller->self.GetObject();
			auto&      host = MCMBridge::NativeFacadeSession();
			const auto token = tokenValue.GetSInt();
			if (!script || token != host.TokenForOwner(reinterpret_cast<std::uintptr_t>(script.get())) || !host.IsActive(token))
				return true;
			if (const auto owned = MCMBridge::NativeHostUI::ResolveCallbackSession(a_frame)) {
				a_result.SetBool(*owned == token);
				return true;
			}
			return true;
		}
	};
}

namespace MCMBridge
{
	bool RegisterNativePageAdmission(RE::BSScript::IVirtualMachine& a_vm)
	{
		RE::BSTSmartPointer<RE::BSScript::IFunction> admission{ new PageAdmission };
		RE::BSTSmartPointer<RE::BSScript::IFunction> config{ new ConfigAdmission };
		const bool                                   pageReady = a_vm.BindNativeMethod(admission.get());
		const bool                                   configReady = a_vm.BindNativeMethod(config.get());
		return pageReady && configReady;
	}
}
