#include "MCMBridge/Papyrus/NativeFacade.h"
#include "MCMBridge/Papyrus/NativeManagerBatch.h"

#include "MCMBridge/Plugin/BridgeController.h"
#include "RE/N/NativeFunctionBase.h"

namespace
{
	using Object = RE::BSTSmartPointer<RE::BSScript::Object>;
	std::int32_t TakeResult(RE::BSScript::IVirtualMachine*, RE::VMStackID a_owner, RE::StaticFunctionTag*, std::int32_t a_request)
	{
		return MCMBridge::NativeRegistryRequests().Take(MCMBridge::NativeFacadeSession().Session(), a_owner, a_request);
	}
	void CancelRequest(RE::BSScript::IVirtualMachine*, RE::VMStackID a_owner, RE::StaticFunctionTag*, std::int32_t a_request)
	{
		MCMBridge::NativeRegistryRequests().Cancel(MCMBridge::NativeFacadeSession().Session(), a_owner, a_request);
	}
	enum class ManagerCall
	{
		kBootstrap,
		kReset,
		kRegister,
		kUnregister
	};

	std::int32_t Execute(ManagerCall a_call, Object a_manager, Object a_menu, std::string a_name)
	{
		auto& controller = MCMBridge::BridgeController::GetSingleton();
		if (a_call == ManagerCall::kBootstrap || a_call == ManagerCall::kReset) {
			auto result = a_call == ManagerCall::kBootstrap ? controller.ActivateNativeHost(std::move(a_manager)) : controller.ResetNativeHost(a_manager);
			if (result)
				return 1;
			if (result.error().code == MCMBridge::BridgeErrorCode::kBusy)
				return -2;
			SKSE::log::error("Native manager bootstrap rejected: {}", result.error().message);
			return -1;
		}
		auto& registry = controller.NativeRegistry();
		if (!controller.IsSessionReady() || !registry.Owns(a_manager))
			return -1;
		const auto revision = registry.Revision();
		auto       result = a_call == ManagerCall::kRegister ? registry.Register(std::move(a_menu), std::move(a_name)) : registry.Unregister(a_menu);
		if (!result) {
			SKSE::log::error("Native manager registration rejected: {}", result.error().message);
			return -1;
		}
		if (registry.Revision() != revision)
			controller.RequestRefresh(true);
		return *result;
	}

	class ManagerFunction final : public RE::BSScript::NF_util::NativeFunctionBase
	{
	public:
		ManagerFunction(std::string_view a_name, ManagerCall a_call, RE::BSTSmartPointer<RE::BSScript::ObjectTypeInfo> a_menuType,
			RE::BSTSmartPointer<RE::BSScript::ObjectTypeInfo> a_managerType) :
			NativeFunctionBase(a_name, "MCMBridgeRegistry", true, a_call == ManagerCall::kRegister ? 3 : a_call == ManagerCall::kUnregister ? 2 :
																																			  1),
			call(a_call), menuType(std::move(a_menuType)), managerType(std::move(a_managerType))
		{
			_retType.SetType(RE::BSScript::TypeInfo::RawType::kInt);
			_descTable.entries[0].first = "a_manager";
			_descTable.entries[0].second.SetType(static_cast<RE::BSScript::TypeInfo::RawType>(reinterpret_cast<std::uintptr_t>(managerType.get())));
			if (call == ManagerCall::kRegister || call == ManagerCall::kUnregister) {
				_descTable.entries[1].first = "a_menu";
				_descTable.entries[1].second.SetType(static_cast<RE::BSScript::TypeInfo::RawType>(reinterpret_cast<std::uintptr_t>(menuType.get())));
			}
			if (call == ManagerCall::kRegister) {
				_descTable.entries[2].first = "a_modName";
				_descTable.entries[2].second.SetType(RE::BSScript::TypeInfo::RawType::kString);
			}
		}

		bool HasStub() const override { return true; }
		bool MarshallAndDispatch(RE::BSScript::Variable&, RE::BSScript::Internal::VirtualMachine&,
			RE::VMStackID a_stackID, RE::BSScript::Variable& a_resultValue, const RE::BSScript::StackFrame& a_frame) const override
		{
			a_resultValue.SetSInt(-1);
			auto* tasks = SKSE::GetTaskInterface();
			if (!tasks)
				return true;
			std::int32_t request = -1;
			const auto   epoch = MCMBridge::NativeFacadeSession().Session();
			try {
				Object      menu;
				std::string name;
				const auto  page = a_frame.GetPageForFrame();
				const auto& base = a_frame.GetStackFrameVariable(0, page);
				if (!base.IsObject())
					return true;
				auto manager = base.GetObject();
				if (call == ManagerCall::kRegister || call == ManagerCall::kUnregister) {
					const auto& value = a_frame.GetStackFrameVariable(1, page);
					if (!value.IsObject())
						return true;
					menu = value.GetObject();
				}
				if (call == ManagerCall::kRegister) {
					const auto& value = a_frame.GetStackFrameVariable(2, page);
					if (!value.IsString())
						return true;
					name = value.GetString();
				}
				if (!epoch) {
					a_resultValue.SetSInt(-2);
					return true;
				}
				request = MCMBridge::NativeRegistryRequests().Submit(epoch, a_stackID);
				if (request < 0)
					return true;
				// Keep the exact script object, not a quest-to-script lookup. Registry
				// mutations are serialized on the game queue without owning a VM stack.
				tasks->AddTask([operation = call, manager = std::move(manager), menu = std::move(menu), name = std::move(name), request, epoch]() mutable {
					if (operation == ManagerCall::kRegister) {
						try {
							MCMBridge::QueueNativeRegistration(std::move(manager), std::move(menu), std::move(name), request, epoch);
						} catch (const std::exception& error) {
							MCMBridge::NativeRegistryRequests().Claim(epoch, request);
							MCMBridge::NativeRegistryRequests().Complete(epoch, request, -1);
							SKSE::log::error("Native registration queue failed: {}", error.what());
						}
						return;
					}
					MCMBridge::FlushNativeRegistrations();
					if (!MCMBridge::NativeRegistryRequests().Claim(epoch, request))
						return;
					std::int32_t result = -1;
					try {
						result = Execute(operation, std::move(manager), std::move(menu), std::move(name));
					} catch (const std::exception& error) {
						SKSE::log::error("Native manager request failed: {}", error.what());
					}
					MCMBridge::NativeRegistryRequests().Complete(epoch, request, result);
				});
				a_resultValue.SetSInt(request);
			} catch (const std::exception& error) {
				MCMBridge::NativeRegistryRequests().Cancel(epoch, a_stackID, request);
				SKSE::log::error("Native manager submission failed: {}", error.what());
			}
			return true;
		}

	private:
		ManagerCall                                       call;
		RE::BSTSmartPointer<RE::BSScript::ObjectTypeInfo> menuType;
		RE::BSTSmartPointer<RE::BSScript::ObjectTypeInfo> managerType;
	};
}

namespace MCMBridge
{
	NativeRegistrationRequests& NativeRegistryRequests()
	{
		static NativeRegistrationRequests requests;
		return requests;
	}

	bool RegisterNativeManager(RE::BSScript::IVirtualMachine* a_vm)
	{
		RE::BSTSmartPointer<RE::BSScript::ObjectTypeInfo> menuType;
		RE::BSTSmartPointer<RE::BSScript::ObjectTypeInfo> managerType;
		if (!a_vm || !a_vm->GetScriptObjectType("SKI_ConfigBase", menuType) || !menuType ||
			!a_vm->GetScriptObjectType("SKI_ConfigManager", managerType) || !managerType)
			return false;
		const bool                                   bootstrap = a_vm->BindNativeMethod(new ManagerFunction("Bootstrap", ManagerCall::kBootstrap, menuType, managerType));
		const bool                                   reset = a_vm->BindNativeMethod(new ManagerFunction("Reset", ManagerCall::kReset, menuType, managerType));
		const bool                                   registration = a_vm->BindNativeMethod(new ManagerFunction("Register", ManagerCall::kRegister, menuType, managerType));
		const bool                                   removal = a_vm->BindNativeMethod(new ManagerFunction("Unregister", ManagerCall::kUnregister, menuType, managerType));
		RE::BSTSmartPointer<RE::BSScript::IFunction> take{ new RE::NativeFunction("TakeResult", "MCMBridgeRegistry", TakeResult) };
		RE::BSTSmartPointer<RE::BSScript::IFunction> cancel{ new RE::NativeFunction("Cancel", "MCMBridgeRegistry", CancelRequest) };
		const bool                                   results = a_vm->BindNativeMethod(take.get());
		const bool                                   cancellation = a_vm->BindNativeMethod(cancel.get());
		if (results)
			a_vm->SetCallableFromTasklets("MCMBridgeRegistry", "TakeResult", true);
		if (cancellation)
			a_vm->SetCallableFromTasklets("MCMBridgeRegistry", "Cancel", true);
		return bootstrap && reset && registration && removal && results && cancellation;
	}
}
