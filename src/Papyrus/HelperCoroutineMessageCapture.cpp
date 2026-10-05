#include "MCMBridge/Papyrus/HelperBinaryAdmission.h"
#include "MCMBridge/Papyrus/HelperMessageCapture.h"

#include "MCMBridge/Core/HelperMenuCapture.h"
#include "MCMBridge/Core/HelperMessageTask.h"
#include "MCMBridge/Papyrus/NativeFacade.h"
#include "MCMBridge/Plugin/BridgeController.h"
#include "MCMBridge/Plugin/TaskScheduler.h"
#include "MCMBridge/UI/MessageDialog.h"

#include <MinHook.h>
#include <limits>

namespace
{
	using Object = RE::BSTSmartPointer<RE::BSScript::Object>;
	struct TextView
	{
		const char* data;
		std::size_t size;
	};
	static_assert(sizeof(TextView) == 16 && sizeof(Object) == 8);

	std::string Copy(TextView a_text)
	{
		if ((!a_text.data && a_text.size) || a_text.size > std::numeric_limits<std::int32_t>::max())
			throw std::invalid_argument("Invalid Helper message text");
		return a_text.size ? std::string(a_text.data, a_text.size) : std::string{};
	}

	// After submission, all mutable request state belongs to the game queue.
	struct Message : std::enable_shared_from_this<Message>
	{
		MCMBridge::HelperMessageTask task;
		Object                       script;
		std::int32_t                 token{};
		std::int32_t                 request{};
		std::uint64_t                timer{};
		std::optional<bool>          result;

		void Drive()
		{
			auto& scheduler = MCMBridge::TaskScheduler::GetSingleton();
			scheduler.Cancel(timer);
			timer = 0;
			auto& host = MCMBridge::NativeFacadeSession();
			if (!host.IsActive(token)) {
				task.Abandon();
				return;
			}
			if (task.Cancelled()) {
				host.CompleteMessage(token, request, false);
				host.TakeMessage(token, request);
				return;
			}
			if (!result) {
				const auto answer = host.TakeMessage(token, request);
				if (answer == -2) {
					// The VM may already have discarded the old stack. Never resume it
					// after timeout/session invalidation merely to report a rejection.
					task.Abandon();
					return;
				}
				if (answer != -1) {
					result = answer == 1;
					if (host.IsActive(token)) {
						if (auto* waiting = script->GetVariable("_waitForMessage"); waiting && waiting->IsBool())
							waiting->SetBool(false);
						if (auto* value = script->GetVariable("_messageResult"); value && value->IsBool())
							value->SetBool(*result);
					}
				}
			}
			if (result && task.Complete(*result))
				return;
			timer = scheduler.Schedule(std::chrono::milliseconds(result ? 1 : 100), [owner = shared_from_this()] { owner->Drive(); });
		}

		void Present(std::vector<std::string> a_text)
		{
			const auto valid = [token = token, request = request] {
				return MCMBridge::NativeFacadeSession().IsMessageActive(token, request);
			};
			if (valid()) {
				const auto complete = [owner = shared_from_this()](bool a_accepted) {
					MCMBridge::NativeFacadeSession().CompleteMessage(owner->token, owner->request, a_accepted);
					owner->Drive();
				};
				try {
					if (!MCMBridge::MessageDialog::Handle(std::move(a_text), complete, valid))
						MCMBridge::NativeFacadeSession().CompleteMessage(token, request, false);
				} catch (const std::exception& error) {
					SKSE::log::error("Native Helper message presentation failed: {}", error.what());
					MCMBridge::NativeFacadeSession().CompleteMessage(token, request, false);
				}
			}
			Drive();
		}
	};

	struct ShowMessage
	{
		// The verified function returns its eight-byte task through RCX (hidden sret).
		static void** thunk(void** a_result, const Object& a_object, TextView a_message,
			bool a_withCancel, TextView a_accept, TextView a_cancel)
		{
			auto&      host = MCMBridge::NativeFacadeSession();
			const auto token = host.TokenForOwner(reinterpret_cast<std::uintptr_t>(a_object.get()));
			if (!token && !MCMBridge::BridgeController::GetSingleton().IsNativeHost())
				return func(a_result, a_object, a_message, a_withCancel, a_accept, a_cancel);
			*a_result = MCMBridge::HelperMessageTask::Rejected();
			auto* tasks = SKSE::GetTaskInterface();
			if (!token || !tasks)
				return a_result;
			std::int32_t request{};
			try {
				auto* waiting = a_object->GetVariable("_waitForMessage");
				auto* value = a_object->GetVariable("_messageResult");
				if (!waiting || !waiting->IsBool() || waiting->GetBool() || !value || !value->IsBool())
					return a_result;
				std::vector<std::string> text{ Copy(a_message), Copy(a_accept), a_withCancel ? Copy(a_cancel) : std::string{} };
				auto                     owner = std::make_shared<Message>();
				owner->script = a_object;
				owner->token = token;
				request = owner->request = host.BeginMessage(token);
				if (!request)
					return a_result;
				waiting->SetBool(true);
				value->SetBool(false);
				*a_result = owner->task.Address();
				tasks->AddTask([owner, text = std::move(text)]() mutable { owner->Present(std::move(text)); });
			} catch (const std::exception& error) {
				if (request) {
					host.CompleteMessage(token, request, false);
					host.TakeMessage(token, request);
					if (auto* waiting = a_object->GetVariable("_waitForMessage"); waiting && waiting->IsBool())
						waiting->SetBool(false);
				}
				*a_result = MCMBridge::HelperMessageTask::Rejected();
				SKSE::log::error("Native Helper message rejected: {}", error.what());
			}
			return a_result;
		}
		static inline decltype(&thunk) func{};
	};
}

namespace MCMBridge
{
	bool InstallHelperCoroutineMessageCapture()
	{
		if (ShowMessage::func)
			return true;
		const auto module = GetModuleHandleW(L"MCMHelper.dll");
		if (!module)
			return true;
		const auto* profile = LoadedHelperBinaryProfile();
		if (!profile || profile->messageABI != HelperMessageABI::kCoroutine)
			return false;
		auto*      base = reinterpret_cast<std::byte*>(module);
		auto*      target = base + profile->Function(HelperHook::kMessage).offset;
		const auto initialized = MH_Initialize();
		if (initialized != MH_OK && initialized != MH_ERROR_ALREADY_INITIALIZED)
			return false;
		void* trampoline{};
		if (MH_CreateHook(target, reinterpret_cast<void*>(&ShowMessage::thunk), &trampoline) != MH_OK)
			return false;
		ShowMessage::func = reinterpret_cast<decltype(ShowMessage::func)>(trampoline);
		if (MH_EnableHook(target) != MH_OK) {
			MH_RemoveHook(target);
			ShowMessage::func = nullptr;
			return false;
		}
		SKSE::log::info("Native Helper message capture installed for {}", profile->name);
		return true;
	}
}
