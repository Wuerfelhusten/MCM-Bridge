#include "MCMBridge/Plugin/JournalRedirect.h"

#include "MCMBridge/Core/JournalRedirect.h"
#include "MCMBridge/Framework/FrameworkApi.h"
#include "MCMBridge/Plugin/BridgeController.h"
#include "MCMBridge/Plugin/BridgeSettingsService.h"
#include "MCMBridge/Plugin/NativeJournalEntry.h"
#include "MCMBridge/Plugin/TaskScheduler.h"
#include "MCMBridge/UI/WriteNotifications.h"
#include "SKSEMenuFramework.h"

namespace
{
	constexpr auto            journalRoot = "_root.QuestJournalFader.Menu_mc";
	constexpr auto            originalMethod = "_mcmBridgeOriginalConfigPanelOpen";
	std::atomic_uint64_t      journalGeneration{};
	thread_local bool         bypass{};
	bool                      installed{};
	std::mutex                handoffMutex;
	MCMBridge::JournalHandoff handoff;

	void FinishHandoff(std::uint64_t a_request)
	{
		auto*      ui = RE::UI::GetSingleton();
		const bool unavailable = !ui || ui->IsMenuOpen(RE::MainMenu::MENU_NAME) ||
		                         ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME);
		bool       open;
		{
			const std::scoped_lock lock(handoffMutex);
			open = handoff.Complete(a_request, journalGeneration.load(),
				ui && ui->IsMenuOpen(RE::JournalMenu::MENU_NAME), unavailable);
		}
		if (open) {
			if (auto* window = SKSEMenuFramework::GetMainWindow()) {
				window->IsOpen.store(true);
				SKSE::log::info("Journal close event completed; opened Menu Framework");
			}
		}
	}

	void ExpireHandoff(std::uint64_t a_request)
	{
		{
			const std::scoped_lock lock(handoffMutex);
			if (!handoff.Expire(a_request))
				return;
		}
		SKSE::log::warn("Journal close timed out; cancelled MCM redirect without forcing input state");
		MCMBridge::WriteNotifications::Show("The Journal did not finish closing. MCM redirection was cancelled.");
	}

	class OpenHandler final : public RE::GFxFunctionHandler
	{
	public:
		void Call(Params& a_params) override
		{
			const bool native = MCMBridge::BridgeController::GetSingleton().IsNativeHost();
			try {
				auto*      window = MCMBridge::FrameworkApi::GetSingleton().IsAvailable() ?
				                        SKSEMenuFramework::GetMainWindow() :
				                        nullptr;
				const auto action = MCMBridge::ResolveJournalRedirect(
					MCMBridge::BridgeSettingsService::GetSingleton().Get(), window != nullptr, bypass, native);
				if (action == MCMBridge::JournalRedirectAction::kUnavailable) {
					SKSE::log::error("Native MCM frontend unavailable; original MCM entry suppressed");
					return;
				}
				if (action == MCMBridge::JournalRedirectAction::kCloseJournal && a_params.thisPtr) {
					std::uint64_t request;
					{
						const std::scoped_lock lock(handoffMutex);
						request = handoff.Begin(journalGeneration.load());
					}
					if (!request)
						return;
					// CloseMenu runs the Journal's native callback, unlike a raw kHide message.
					if (a_params.thisPtr->Invoke("CloseMenu", nullptr, nullptr, 0)) {
						SKSE::log::info("Requested native Journal close for MCM redirect");
						MCMBridge::TaskScheduler::GetSingleton().After(std::chrono::seconds(2),
							[request] { ExpireHandoff(request); });
						return;
					}
					{
						const std::scoped_lock lock(handoffMutex);
						handoff.Cancel();
					}
					if (native) {
						window->IsOpen.store(true);
						SKSE::log::warn("Journal close failed; opened native MCM frontend with Journal retained");
						return;
					}
					SKSE::log::warn("Journal CloseMenu is unavailable; retaining the original MCM entry");
				} else if (action == MCMBridge::JournalRedirectAction::kKeepJournal) {
					window->IsOpen.store(true);
					SKSE::log::info("Redirected Journal MCM entry to Menu Framework; keeping Journal open");
					return;
				}
			} catch (...) {
				{
					const std::scoped_lock lock(handoffMutex);
					handoff.Cancel();
				}
				SKSE::log::error("Journal MCM redirect failed; original menu {}", native ? "suppressed by native host" : "retained");
			}
			if (!native && a_params.thisPtr) {
				MCMBridge::BridgeController::GetSingleton().NotifyOriginalMCMState(true);
				a_params.thisPtr->Invoke(originalMethod, a_params.retVal, a_params.args, a_params.argCount);
			}
		}
	};

	bool Attach(RE::GFxMovieView& a_movie)
	{
		RE::GFxValue root;
		RE::GFxValue original;
		if (!a_movie.GetVariable(&root, journalRoot) || !root.IsObject())
			return false;
		if (root.GetMember(originalMethod, &original) && original.IsObject())
			return true;
		const bool   existing = root.GetMember("ConfigPanelOpen", &original) && original.IsObject();
		RE::GFxValue wrapper;
		auto*        handler = new OpenHandler;
		a_movie.CreateFunction(&wrapper, handler);
		handler->Release();
		if (!existing)
			return wrapper.IsObject() && MCMBridge::AttachNativeJournalEntry(a_movie, root, wrapper);
		if (!wrapper.IsObject() || !root.SetMember(originalMethod, original))
			return false;
		if (!root.SetMember("ConfigPanelOpen", wrapper)) {
			root.SetMember(originalMethod, RE::GFxValue());
			return false;
		}
		SKSE::log::debug("Attached Journal MCM redirect");
		return true;
	}

	void AttachCurrent(std::uint64_t a_generation, std::uint32_t a_attempt)
	{
		if (a_generation != journalGeneration.load())
			return;
		auto* ui = RE::UI::GetSingleton();
		auto  movie = ui ? ui->GetMovieView(RE::JournalMenu::MENU_NAME) : nullptr;
		if (movie && Attach(*movie))
			return;
		if (a_attempt >= 20) {
			SKSE::log::warn("Journal has no compatible ConfigPanelOpen entry; MCM redirect was not installed");
			return;
		}
		MCMBridge::TaskScheduler::GetSingleton().After(std::chrono::milliseconds(50),
			[a_generation, a_attempt] { AttachCurrent(a_generation, a_attempt + 1); });
	}

	class MenuEvents final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(
			const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
		{
			if (!a_event)
				return RE::BSEventNotifyControl::kContinue;
			if (a_event->menuName == RE::JournalMenu::MENU_NAME) {
				if (!a_event->opening) {
					MCMBridge::BridgeController::GetSingleton().NotifyOriginalMCMState(false);
				}
				const auto    generation = ++journalGeneration;
				std::uint64_t request{};
				{
					const std::scoped_lock lock(handoffMutex);
					if (a_event->opening)
						handoff.Cancel();
					else
						request = handoff.JournalClosed(generation);
				}
				if (a_event->opening)
					AttachCurrent(generation, 0);
				if (request) {
					if (auto* tasks = SKSE::GetTaskInterface()) {
						tasks->AddTask([request] { FinishHandoff(request); });
					} else {
						const std::scoped_lock lock(handoffMutex);
						handoff.Cancel();
					}
				}
			} else if (a_event->opening && (a_event->menuName == RE::MainMenu::MENU_NAME ||
											   a_event->menuName == RE::LoadingMenu::MENU_NAME)) {
				const std::scoped_lock lock(handoffMutex);
				handoff.Cancel();
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	};
	MenuEvents menuEvents;
}

namespace MCMBridge::JournalRedirect
{
	bool Install()
	{
		if (installed)
			return true;
		auto* ui = RE::UI::GetSingleton();
		if (!ui)
			return false;
		ui->AddEventSink<RE::MenuOpenCloseEvent>(&menuEvents);
		installed = true;
		return true;
	}

	bool OpenOriginalPanel(RE::GFxMovie& a_movie)
	{
		if (BridgeController::GetSingleton().IsNativeHost())
			return false;
		struct BypassScope
		{
			bool previous{ std::exchange(bypass, true) };
			~BypassScope() { bypass = previous; }
		} scope;
		return a_movie.Invoke("_root.QuestJournalFader.Menu_mc.ConfigPanelOpen", nullptr, nullptr, 0);
	}
}
