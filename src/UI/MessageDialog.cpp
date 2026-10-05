#include "MCMBridge/UI/MessageDialog.h"

#include "MCMBridge/Core/SkyUIRichText.h"
#include "MCMBridge/Localization/SnapshotLocalizer.h"
#include "MCMBridge/Plugin/BridgeController.h"
#include "SKSEMenuFramework.h"

#include <limits>
#include <mutex>
#include <optional>

namespace
{
	struct Request
	{
		std::uint64_t             id{};
		std::uint64_t             edit{};
		std::function<void(bool)> completion;
		std::function<bool()>     valid;
		std::string               message;
		std::string               accept;
		std::string               cancel;
	};

	std::mutex                                 requestMutex;
	std::optional<Request>                     pending;
	SKSEMenuFramework::Model::WindowInterface* window{};
	std::uint64_t                              nextRequest{};

	void Complete(bool a_accepted, std::uint64_t a_expected = 0)
	{
		std::function<void(bool)> completion;
		std::uint64_t             edit{};
		{
			const std::scoped_lock lock(requestMutex);
			if (!pending || (a_expected && pending->id != a_expected)) {
				return;
			}
			completion = std::move(pending->completion);
			edit = pending->edit;
			pending.reset();
			if (window)
				window->IsOpen.store(false);
		}
		auto* tasks = SKSE::GetTaskInterface();
		if (!tasks) {
			return;
		}
		tasks->AddTask([a_accepted, edit, completion = std::move(completion)] {
			MCMBridge::BridgeController::GetSingleton().ObserveUserConfirmation(edit, a_accepted);
			if (completion) {
				completion(a_accepted);
				return;
			}
			auto* source = SKSE::GetModCallbackEventSource();
			if (!source) {
				return;
			}
			SKSE::ModCallbackEvent event{
				RE::BSFixedString("SKICP_messageDialogClosed"),
				RE::BSFixedString(),
				a_accepted ? 1.0F : 0.0F,
				nullptr
			};
			source->SendEvent(std::addressof(event));
		});
	}
}

namespace MCMBridge::MessageDialog
{
	bool Handle(std::vector<std::string> a_arguments, std::function<void(bool)> a_completion, std::function<bool()> a_valid)
	{
		if (a_completion && (!a_valid || a_valid())) {
			if (const auto response = BridgeController::GetSingleton().HostMessageResponse(a_arguments.size() > 2 && !a_arguments[2].empty())) {
				a_completion(*response);
				return true;
			}
		}
		Request request;
		request.edit = a_arguments.size() > 2 && !a_arguments[2].empty() ? BridgeController::GetSingleton().UserEditID() : 0;
		request.completion = std::move(a_completion);
		request.valid = std::move(a_valid);
		request.message = a_arguments.empty() ? std::string{} : SnapshotLocalizer::LocalizeText(std::move(a_arguments[0]));
		request.accept = a_arguments.size() > 1 ? SnapshotLocalizer::LocalizeText(std::move(a_arguments[1])) : "OK";
		request.cancel = a_arguments.size() > 2 ? SnapshotLocalizer::LocalizeText(std::move(a_arguments[2])) : "Cancel";
		bool          completeImmediately{};
		std::uint64_t id{};
		{
			const std::scoped_lock lock(requestMutex);
			if (pending || nextRequest == std::numeric_limits<std::uint64_t>::max())
				return false;
			request.id = id = ++nextRequest;
			completeImmediately = !window;
			pending = std::move(request);
			if (window)
				window->IsOpen.store(true);
		}
		if (completeImmediately) {
			SKSE::log::warn("Rejected an MCM message because the message window is unavailable");
			Complete(false, id);
		}
		return true;
	}

	static void __stdcall Render()
	{
		static std::uint64_t   focusedRequest{};
		std::optional<Request> request;
		{
			const std::scoped_lock lock(requestMutex);
			if (!pending) {
				return;
			}
			request = pending;
		}
		if (request->valid && !request->valid()) {
			Complete(false, request->id);
			return;
		}
		bool open = true;
		if (focusedRequest != request->id) {
			focusedRequest = request->id;
			if (const auto* viewport = ImGuiMCP::GetMainViewport())
				ImGuiMCP::SetNextWindowPos({ viewport->WorkPos.x + viewport->WorkSize.x * 0.5F,
											   viewport->WorkPos.y + viewport->WorkSize.y * 0.5F },
					ImGuiMCP::ImGuiCond_Always, { 0.5F, 0.5F });
			ImGuiMCP::SetNextWindowFocus();
		}
		ImGuiMCP::SetNextWindowSize({ 580.0F, 0.0F }, ImGuiMCP::ImGuiCond_FirstUseEver);
		if (ImGuiMCP::Begin("MCM Bridge - Message", &open, ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize)) {
			const auto message = PlainSkyUIText(request->message);
			ImGuiMCP::TextWrapped("%s", message.c_str());
			const auto accept = PlainSkyUIText(request->accept);
			if (ImGuiMCP::Button(accept.empty() ? "OK" : accept.c_str())) {
				Complete(true, request->id);
			}
			const auto cancel = PlainSkyUIText(request->cancel);
			if (!cancel.empty()) {
				ImGuiMCP::SameLine();
				if (ImGuiMCP::Button(cancel.c_str())) {
					Complete(false, request->id);
				}
			}
		}
		ImGuiMCP::End();
		if (!open)
			Complete(false, request->id);
	}

	bool Install()
	{
		const std::scoped_lock lock(requestMutex);
		if (!window)
			window = SKSEMenuFramework::AddWindow(Render, true);
		return window != nullptr;
	}

	void Cancel()
	{
		Complete(false);
	}
}
