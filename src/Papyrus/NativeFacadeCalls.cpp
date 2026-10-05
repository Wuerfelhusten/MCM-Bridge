#include "MCMBridge/Papyrus/NativeFacade.h"
#include "MCMBridge/Papyrus/NativeFacadeBinding.h"
#include "MCMBridge/Papyrus/NativeHostUI.h"
#include "MCMBridge/Plugin/BridgeController.h"

namespace
{
	constexpr std::array<std::string_view, 14> methods{
		"OpenConfig", "CloseConfig", "SetPage", "SelectOption", "ResetOption", "HighlightOption",
		"RequestSliderDialogData", "RequestMenuDialogData", "RequestColorDialogData", "RequestInputDialogData",
		"SetSliderValue", "SetMenuIndex", "SetColorValue", "SetInputText"
	};

	const RE::BSScript::StackFrame* Caller(const RE::BSScript::StackFrame& a_frame, std::string_view a_helper)
	{
		const auto* helper = a_frame.previousFrame;
		const auto* caller = helper ? helper->previousFrame : nullptr;
		if (!helper || !helper->owningFunction || helper->owningFunction->GetName() != a_helper ||
			helper->owningFunction->GetObjectTypeName() != "SKI_ConfigBase" || !caller || !caller->owningFunction ||
			caller->owningFunction->GetObjectTypeName() != "SKI_ConfigBase" || !caller->self.IsObject() || !a_frame.parent)
			return nullptr;
		const std::string_view method(caller->owningFunction->GetName().c_str());
		return std::ranges::find(methods, method) != methods.end() || method == "RemapKey" ? caller : nullptr;
	}

	class CallAdmission final : public RE::NativeFunction<std::int32_t(RE::StaticFunctionTag*)>
	{
		using Base = RE::NativeFunction<std::int32_t(RE::StaticFunctionTag*)>;

	public:
		CallAdmission() : Base("EnterCall", "MCMBridgeNative", [](RE::StaticFunctionTag*) { return 0; }) { SetCallableFromTasklets(true); }
		bool MarshallAndDispatch(RE::BSScript::Variable&, RE::BSScript::Internal::VirtualMachine&, RE::VMStackID a_stack,
			RE::BSScript::Variable& a_result, const RE::BSScript::StackFrame& a_frame) const override
		{
			a_result.SetSInt(0);
			const auto* caller = Caller(a_frame, "BridgeEnterCall");
			if (!caller)
				return true;
			const auto script = caller->self.GetObject();
			if (!script)
				return true;
			const auto owned = MCMBridge::NativeHostUI::ResolveCallbackSession(a_frame);
			if (owned) {
				auto& host = MCMBridge::NativeFacadeSession();
				if (!host.IsActive(*owned) || host.TokenForOwner(reinterpret_cast<std::uintptr_t>(script.get())) != *owned)
					return true;
				if (!MCMBridge::NativeHostUI::IsScriptStack(a_frame)) {
					a_result.SetSInt(-1);
					return true;
				}
			}
			auto* tasks = SKSE::GetTaskInterface();
			if (!tasks)
				return true;
			const auto session = MCMBridge::NativeFacadeSession().Session();
			const auto request = MCMBridge::NativeCallRequests().Submit(session, a_stack);
			if (request <= 0)
				return true;
			RE::BSTSmartPointer<RE::BSScript::Stack> stack{ a_frame.parent };
			tasks->AddTask([session, request, script, stack = std::move(stack), method = std::string(caller->owningFunction->GetName().c_str())] {
				MCMBridge::BridgeController::GetSingleton().BeginScriptCall(session, request, script, stack, method);
			});
			a_result.SetSInt(request);
			return true;
		}
	};

	std::int32_t TakeCall(RE::BSScript::IVirtualMachine*, RE::VMStackID a_stack, RE::StaticFunctionTag*, std::int32_t a_request)
	{
		return MCMBridge::NativeCallRequests().Take(MCMBridge::NativeFacadeSession().Session(), a_stack, a_request);
	}
	std::int32_t LeaveCall(RE::BSScript::IVirtualMachine*, RE::VMStackID a_stack, RE::StaticFunctionTag*, std::int32_t a_admission, bool a_valid)
	{
		auto* tasks = SKSE::GetTaskInterface();
		if (!tasks)
			return 0;
		const auto session = MCMBridge::NativeFacadeSession().Session();
		const auto request = MCMBridge::NativeCallRequests().Submit(session, a_stack);
		if (request > 0)
			tasks->AddTask([session, request, a_admission, a_stack, a_valid] {
				MCMBridge::BridgeController::GetSingleton().FinishScriptCall(session, request, a_admission, a_stack, a_valid);
			});
		return request;
	}
}

namespace MCMBridge
{
	NativeRegistrationRequests& NativeCallRequests()
	{
		static NativeRegistrationRequests requests;
		return requests;
	}
	bool RegisterNativeCallAdmission(RE::BSScript::IVirtualMachine& a_vm)
	{
		NativeFacadeBinding binding(a_vm);
		binding.RegisterFunction("TakeCall", TakeCall);
		binding.RegisterFunction("LeaveCall", LeaveCall);
		RE::BSTSmartPointer<RE::BSScript::IFunction> admission{ new CallAdmission };
		return a_vm.BindNativeMethod(admission.get()) && binding.IsValid();
	}
}
