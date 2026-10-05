#include "MCMBridge/Papyrus/HelperMessageCapture.h"
#include "MCMBridge/Papyrus/HelperBinaryAdmission.h"

#include "MCMBridge/Core/HelperMenuCapture.h"
#include "MCMBridge/Core/HelperMessageCallback.h"
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
		explicit Message(void* a_callback) : callback(a_callback) {}
		MCMBridge::HelperMessageCallback callback;
		Object                           script;
		std::int32_t                     token{};
		std::int32_t                     request{};
		std::uint64_t                    timer{};
		std::optional<bool>              result;

		void Drive()
		{
			auto& scheduler = MCMBridge::TaskScheduler::GetSingleton();
			scheduler.Cancel(timer);
			timer = 0;
			auto& host = MCMBridge::NativeFacadeSession();
			if (!host.IsActive(token)) {
				callback.Abandon();
				return;
			}
			if (!result) {
				const auto answer = host.TakeMessage(token, request);
				if (answer == -2) {
					// The VM may already have discarded the old stack. Never resume it
					// after timeout/session invalidation merely to report a rejection.
					callback.Abandon();
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
			if (result) {
				try {
					callback.Complete(*result);
				} catch (const std::exception& error) {
					SKSE::log::error("Native Helper message callback failed: {}", error.what());
				}
				return;
			}
			timer = scheduler.Schedule(std::chrono::milliseconds(100), [owner = shared_from_this()] { owner->Drive(); });
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
		// The by-value 64-byte callback is passed indirectly on Windows x64.
		// The native host consumes the argument even when the execution owner expired.
		static void thunk(const Object& a_object, TextView a_message,
			bool a_withCancel, TextView a_accept, TextView a_cancel, void* a_callback)
		{
			auto&      host = MCMBridge::NativeFacadeSession();
			const auto token = host.TokenForOwner(reinterpret_cast<std::uintptr_t>(a_object.get()));
			const auto consume = std::unique_ptr<void, decltype(&MCMBridge::HelperMessageCallback::DestroyArgument)>(
				a_callback, &MCMBridge::HelperMessageCallback::DestroyArgument);
			auto* tasks = SKSE::GetTaskInterface();
			if (!token || !tasks)
				return;
			std::int32_t             request{};
			std::shared_ptr<Message> owner;
			const auto               reject = [&] {
				if (owner)
					tasks->AddTask([owner] {
						if (MCMBridge::NativeFacadeSession().IsActive(owner->token))
							owner->callback.Complete(false);
					});
			};
			try {
				owner = std::make_shared<Message>(a_callback);
				owner->script = a_object;
				owner->token = token;
				auto* waiting = a_object->GetVariable("_waitForMessage");
				auto* value = a_object->GetVariable("_messageResult");
				if (!waiting || !waiting->IsBool() || waiting->GetBool() || !value || !value->IsBool()) {
					reject();
					return;
				}
				std::vector<std::string> text{ Copy(a_message), Copy(a_accept), a_withCancel ? Copy(a_cancel) : std::string{} };
				request = owner->request = host.BeginMessage(token);
				if (!request) {
					reject();
					return;
				}
				waiting->SetBool(true);
				value->SetBool(false);
				tasks->AddTask([owner, text = std::move(text)]() mutable { owner->Present(std::move(text)); });
			} catch (const std::exception& error) {
				if (request) {
					host.CompleteMessage(token, request, false);
					host.TakeMessage(token, request);
					if (auto* waiting = a_object->GetVariable("_waitForMessage"); waiting && waiting->IsBool())
						waiting->SetBool(false);
				}
				reject();
				SKSE::log::error("Native Helper message rejected: {}", error.what());
			}
		}
		static inline decltype(&thunk) func{};
	};
}

namespace MCMBridge
{
	bool InstallHelperCoroutineMessageCapture();

	bool InstallHelperMessageCapture()
	{
		if (ShowMessage::func)
			return true;
		const auto module = GetModuleHandleW(L"MCMHelper.dll");
		if (!module)
			return true;
		const auto* profile = LoadedHelperBinaryProfile();
		if (!profile)
			return false;
		auto* base = reinterpret_cast<std::byte*>(module);
		if (profile->messageABI == HelperMessageABI::kCoroutine)
			return InstallHelperCoroutineMessageCapture();
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
