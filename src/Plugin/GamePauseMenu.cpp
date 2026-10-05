#include "MCMBridge/Plugin/GamePauseMenu.h"

#include "MCMBridge/Plugin/BridgeController.h"
#include "MCMBridge/Plugin/WritePauseService.h"

#include <atomic>

namespace
{
	constexpr std::string_view menuName = "MCMBridgeWritePauseMenu";
	std::atomic_bool           wanted{};
	std::atomic_bool           requested{};
	bool                       installed{};

	class PauseMenu final : public RE::IMenu
	{
	public:
		PauseMenu()
		{
			menuFlags.set(RE::UI_MENU_FLAGS::kPausesGame, RE::UI_MENU_FLAGS::kDontHideCursorWhenTopmost);
			depthPriority = 0;
		}

		RE::UI_MESSAGE_RESULTS ProcessMessage(RE::UIMessage& a_message) override
		{
			// A newly queued write can revoke a close before the UI consumes it.
			if (a_message.type == RE::UI_MESSAGE_TYPE::kHide && wanted.load()) {
				requested.store(true);
				MCMBridge::WritePauseService::GetSingleton().Reconcile();
				return RE::UI_MESSAGE_RESULTS::kIgnore;
			}
			return RE::UI_MESSAGE_RESULTS::kHandled;
		}

		void AdvanceMovie(float, std::uint32_t) override {}
		void PostDisplay() override {}
	};

	class MenuEvents final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(
			const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
		{
			if (a_event && !a_event->opening && a_event->menuName == RE::JournalMenu::MENU_NAME && wanted.load()) {
				SKSE::log::info("Journal Menu closed; retaining the pause for pending setting changes");
			}
			if (a_event && a_event->menuName == menuName) {
				SKSE::log::info("Setting pause menu {}", a_event->opening ? "opened" : "closed");
				if (a_event->opening)
					if (!a_event->opening) {
						requested.store(false);
						MCMBridge::WritePauseService::GetSingleton().MenuClosed();
					}
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	};
	MenuEvents menuEvents;
}

namespace MCMBridge::GamePauseMenu
{
	bool Install()
	{
		if (installed)
			return true;
		auto* ui = RE::UI::GetSingleton();
		if (!ui)
			return false;
		ui->Register(menuName, []() -> RE::IMenu* { return new PauseMenu; });
		ui->AddEventSink<RE::MenuOpenCloseEvent>(&menuEvents);
		installed = true;
		return true;
	}

	void SetWanted(bool a_value) { wanted.store(a_value); }

	void Synchronize()
	{
		auto* queue = RE::UIMessageQueue::GetSingleton();
		if (!installed || !queue)
			return;
		const auto value = wanted.load();
		if (requested.exchange(value) != value) {
			if (value && IsJournalOpen()) {
				SKSE::log::info("Journal Menu is already open; retaining its pause across setting changes");
			}
			queue->AddMessage(RE::BSFixedString(menuName),
				value ? RE::UI_MESSAGE_TYPE::kShow : RE::UI_MESSAGE_TYPE::kHide, nullptr);
		}
	}

	bool IsOpen()
	{
		auto*      ui = RE::UI::GetSingleton();
		const auto menu = ui ? ui->GetMenu(menuName) : RE::GPtr<RE::IMenu>{};
		return menu && menu->OnStack() && menu->PausesGame() && ui->GameIsPaused();
	}

	bool IsJournalOpen()
	{
		const auto ui = RE::UI::GetSingleton();
		return ui && ui->IsMenuOpen(RE::JournalMenu::MENU_NAME);
	}
}
