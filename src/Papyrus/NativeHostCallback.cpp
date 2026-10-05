#include "MCMBridge/Papyrus/NativeHostCallback.h"

#include <atomic>
#include <unordered_map>

namespace
{
	std::mutex                                                                                     callbacksMutex;
	std::unordered_map<const RE::BSScript::IStackCallbackFunctor*, MCMBridge::NativeHostCallback*> callbacks;
}

namespace MCMBridge
{
	NativeHostCallback::NativeHostCallback(RE::BSTSmartPointer<RE::BSScript::Object> a_script, std::string a_menu, std::string a_root, Receiver a_receiver) :
		script(std::move(a_script)), menu(std::move(a_menu)), root(std::move(a_root)), receiver(std::move(a_receiver))
	{
		root += '.';
		static std::atomic<std::uint64_t> nextSession{ 1 };
		token = capture.Begin(nextSession.fetch_add(1), "", -1);
		const std::scoped_lock lock(callbacksMutex);
		callbacks.emplace(this, this);
	}

	NativeHostCallback::~NativeHostCallback()
	{
		const std::scoped_lock lock(callbacksMutex);
		callbacks.erase(this);
	}

	NativeHostCallback* NativeHostCallback::Find(const RE::BSScript::IStackCallbackFunctor* a_callback)
	{
		// The caller holds the VM callback reference while using this pointer.
		const std::scoped_lock lock(callbacksMutex);
		const auto             found = callbacks.find(a_callback);
		return found != callbacks.end() ? found->second : nullptr;
	}

	bool NativeHostCallback::Observe(const RE::BSScript::StackFrame& a_frame, std::string_view a_menu, std::string_view a_target, HostBufferPayload a_payload, HostProtocolCallKind a_kind)
	{
		if (a_menu != menu || !a_target.starts_with(root))
			return false;
		// An operation can call other scripts. The nearest object frame, not merely
		// an ancestor somewhere in the stack, must belong to the bound MCM instance.
		const auto* caller = a_frame.previousFrame;
		while (caller && !caller->self.IsObject())
			caller = caller->previousFrame;
		if (!caller || caller->self.GetObject() != script)
			return false;
		const std::scoped_lock lock(mutex);
		const auto             target = a_target.substr(root.size());
		if (a_kind == HostProtocolCallKind::kInvoke)
			capture.Observe(token, target, a_payload);
		return controls.Observe(a_kind, target, a_payload);
	}

	void NativeHostCallback::operator()(RE::BSScript::Variable)
	{
		const auto            completed = std::chrono::steady_clock::now();
		NativeHostObservation observation;
		Receiver              deliver;
		{
			const std::scoped_lock lock(mutex);
			observation.page = capture.Complete(token);
			observation.changes = controls.Complete();
			deliver = std::move(receiver);
		}
		observation.dispatchTime = completed - started;
		if (auto* tasks = SKSE::GetTaskInterface(); tasks && deliver) {
			tasks->AddTask([deliver = std::move(deliver), observation = std::move(observation), completed]() mutable {
				observation.taskWait = std::chrono::steady_clock::now() - completed;
				deliver(std::move(observation));
			});
		}
	}
}
