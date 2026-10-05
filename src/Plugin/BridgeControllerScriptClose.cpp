#include "MCMBridge/Papyrus/NativeFacade.h"
#include "MCMBridge/Plugin/BridgeController.h"

#include "MCMBridge/Papyrus/NativeHostUI.h"
#include "MCMBridge/Plugin/TaskScheduler.h"

namespace
{
	class ScriptCloseCompletion final : public RE::BSScript::IStackCallbackFunctor
	{
	public:
		explicit ScriptCloseCompletion(std::function<void()> a_receiver) : receiver(std::move(a_receiver)) {}
		~ScriptCloseCompletion() override { MCMBridge::NativeHostUI::Detach(this); }
		void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
		void operator()(RE::BSScript::Variable) override
		{
			if (auto* tasks = SKSE::GetTaskInterface(); tasks && receiver)
				tasks->AddTask(std::exchange(receiver, {}));
		}

	private:
		std::function<void()> receiver;
	};
}

namespace MCMBridge
{
	void BridgeController::CloseScriptContext()
	{
		if (!scriptContext.lease || scriptContext.calls.Busy() || scriptContext.closing)
			return;
		scriptContext.closing = true;
		const auto generation = session;
		const auto token = scriptContext.calls.Token();
		TaskScheduler::GetSingleton().Cancel(scriptContext.timer);
		scriptContext.executionDeadline = std::make_unique<OperationDeadline>(TaskScheduler::GetSingleton());
		scriptContext.executionDeadline->Arm(std::chrono::seconds(30), [this, generation, token] {
			if (session == generation && scriptContext.closing && scriptContext.calls.Token() == token)
				RetireScriptContext(true); }, [token] { return NativeFacadeSession().MessageWaitDuration(token); });
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback{ new ScriptCloseCompletion([this, generation, token] {
			if (session == generation && scriptContext.closing && scriptContext.calls.Token() == token)
				RetireScriptContext();
		}) };
		NativeHostUI::Attach(callback.get(), token, scriptContext.script.get());
		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		SKSE::log::info("Closing idle external Papyrus config: mod={}", scriptContext.mod);
		if (!vm || !vm->DispatchMethodCall(scriptContext.script, "CloseConfig", RE::MakeFunctionArguments(), callback)) {
			quarantined.insert(scriptContext.mod);
			SKSE::log::error("External Papyrus config cleanup dispatch failed: mod={}", scriptContext.mod);
			RetireScriptContext();
		}
	}
}
