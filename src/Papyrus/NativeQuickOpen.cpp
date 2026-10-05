#include "MCMBridge/Core/QuickOpenPage.h"
#include "MCMBridge/Papyrus/NativeHostUI.h"

#include "MCMBridge/Papyrus/NativeFacade.h"
#include "MCMBridge/Plugin/BridgeController.h"
#include "MCMBridge/UI/QuickOpenWindow.h"

namespace
{
	using NativeBase = RE::BSScript::NF_util::NativeFunctionBase;

	bool Redirect(const RE::BSScript::StackFrame& a_frame)
	{
		if (!MCMBridge::BridgeController::GetSingleton().IsNativeHost())
			return false;
		const auto* caller = a_frame.previousFrame;
		if (!caller || !caller->owningFunction || !caller->self.IsObject() ||
			caller->owningFunction->GetObjectTypeName() != "nl_mcm" ||
			(caller->owningFunction->GetName() != "OpenMCM" && caller->owningFunction->GetName() != "OnKeyDown"))
			return false;
		auto        script = caller->self.GetObject();
		auto*       quick = script ? script->GetVariable("_quick_open") : nullptr;
		const auto* temporary = script ? script->GetVariable("_landing_page_tmp") : nullptr;
		const auto* landing = script ? script->GetVariable("_landing_page") : nullptr;
		if (!quick || !quick->IsBool() || !quick->GetBool() || !temporary || !temporary->IsString() || !landing || !landing->IsString())
			return false;
		std::string page(temporary->GetString());
		if (page.empty())
			page = landing->GetString();
		const auto session = MCMBridge::NativeFacadeSession().Session();
		const bool hotkey = caller->owningFunction->GetName() == "OnKeyDown";
		// Consume the frontend handshake on its own VM stack. A later Journal
		// event must not run the old search loop. No mod setting is changed here.
		quick->SetBool(false);
		if (auto* tasks = SKSE::GetTaskInterface()) {
			tasks->AddTask([script = std::move(script), page = std::move(page), session, hotkey]() mutable {
				auto& controller = MCMBridge::BridgeController::GetSingleton();
				if (!controller.IsNativeHost() || !controller.IsSessionReady() || MCMBridge::NativeFacadeSession().Session() != session)
					return;
				const auto identity = controller.NativeRegistry().ResolveIdentity(script);
				if (!identity) {
					SKSE::log::warn("Native quick-open rejected: {}", identity.error().message);
					return;
				}
				if (hotkey) {
					if (const auto revision = controller.ScriptViewRevision(session, *identity)) {
						controller.CloseScriptView(session, *revision, *identity, true);
						return;
					}
				}
				controller.ClearHostedPageRoute();
				const auto snapshot = controller.Snapshot();
				const auto mod = std::ranges::find(snapshot->mods, *identity, &MCMBridge::MCMMod::stableID);
				if (mod == snapshot->mods.end() || !MCMBridge::ResolveQuickOpenPage(*mod, page))
					controller.RequestRefresh(true);
				MCMBridge::QuickOpenWindow::Open(session, *identity, page);
				SKSE::log::info("Native quick-open: mod={} page=\"{}\"", *identity, page);
			});
		}
		return true;
	}

	class TapKey final : public RE::NativeFunction<void(RE::StaticFunctionTag*, std::int32_t)>
	{
		using Base = RE::NativeFunction<void(RE::StaticFunctionTag*, std::int32_t)>;

	public:
		explicit TapKey(RE::BSTSmartPointer<RE::BSScript::IFunction> a_original) :
			Base("TapKey", "Input", [](RE::StaticFunctionTag*, std::int32_t) {}), original(std::move(a_original))
		{
			SetCallableFromTasklets(original->CanBeCalledFromTasklets());
		}
		bool MarshallAndDispatch(RE::BSScript::Variable& a_base, RE::BSScript::Internal::VirtualMachine& a_vm,
			RE::VMStackID a_stackID, RE::BSScript::Variable& a_result, const RE::BSScript::StackFrame& a_frame) const override
		{
			if (Redirect(a_frame))
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
				if (functions[index].func && functions[index].func->GetName() == "TapKey")
					return functions[index].func;
		} else {
			for (const auto* entry = a_type.GetUnlinkedFunctionIter(); entry; entry = entry->next)
				if (entry->func && entry->func->GetName() == "TapKey")
					return entry->func;
		}
		return {};
	}
}

namespace MCMBridge::NativeHostUI
{
	bool RegisterQuickOpen(RE::BSScript::IVirtualMachine& a_vm)
	{
		RE::BSTSmartPointer<RE::BSScript::ObjectTypeInfo> input;
		if (!a_vm.GetScriptObjectType("Input", input) || !input)
			return false;
		auto original = Find(*input);
		if (!original || !original->GetIsNative() || !original->GetIsStatic() || original->GetParamCount() != 1 ||
			static_cast<NativeBase*>(original.get())->GetIsLatent())
			return false;
		RE::BSTSmartPointer<RE::BSScript::IFunction> replacement{ new TapKey(original) };
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
