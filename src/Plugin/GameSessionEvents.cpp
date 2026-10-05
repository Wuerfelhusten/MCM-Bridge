#include "MCMBridge/Plugin/GameSessionEvents.h"

#include "MCMBridge/Core/SessionLifecycle.h"
#include "MCMBridge/Plugin/BridgeController.h"

#include <mutex>

namespace
{
	std::mutex                  mutex;
	MCMBridge::SessionLifecycle lifecycle;
	bool                        installed{};

	bool BlockingMenuOpen()
	{
		auto* ui = RE::UI::GetSingleton();
		return !ui || ui->IsMenuOpen(RE::MainMenu::MENU_NAME) || ui->IsMenuOpen(RE::LoadingMenu::MENU_NAME) ||
		       ui->IsMenuOpen(RE::RaceSexMenu::MENU_NAME);
	}

	bool PlayerInCell()
	{
		const auto* player = RE::PlayerCharacter::GetSingleton();
		return player && player->GetParentCell();
	}

	void ResetIfCurrent(std::uint64_t a_token)
	{
		auto*      ui = RE::UI::GetSingleton();
		const auto mainMenuOpen = ui && ui->IsMenuOpen(RE::MainMenu::MENU_NAME);
		{
			const std::scoped_lock lock(mutex);
			if (!lifecycle.IsCurrentMainMenuEvent(a_token, mainMenuOpen)) {
				SKSE::log::debug("Ignored a stale main menu session reset");
				return;
			}
		}
		MCMBridge::BridgeController::GetSingleton().StartSession("Returned to main menu", false);
	}

	class MenuEvents final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
	{
	public:
		RE::BSEventNotifyControl ProcessEvent(
			const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
		{
			if (!a_event)
				return RE::BSEventNotifyControl::kContinue;
			if (a_event->menuName == RE::MainMenu::MENU_NAME) {
				std::uint64_t token;
				{
					const std::scoped_lock lock(mutex);
					token = lifecycle.ObserveMainMenu(a_event->opening);
				}
				if (a_event->opening) {
					if (auto* tasks = SKSE::GetTaskInterface()) {
						tasks->AddTask([token] { ResetIfCurrent(token); });
					}
				} else {
					MCMBridge::BridgeController::GetSingleton().RequestRefresh(true);
				}
			} else if ((a_event->menuName == RE::LoadingMenu::MENU_NAME || a_event->menuName == RE::RaceSexMenu::MENU_NAME) && !a_event->opening) {
				bool needsActivation;
				{
					const std::scoped_lock lock(mutex);
					needsActivation = lifecycle.NeedsActivation();
				}
				if (needsActivation)
					MCMBridge::BridgeController::GetSingleton().RequestRefresh(true);
				MCMBridge::BridgeController::GetSingleton().ResumeGameUI();
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	};
	MenuEvents menuEvents;
}

namespace MCMBridge::GameSessionEvents
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

	void BeginLoad()
	{
		const std::scoped_lock lock(mutex);
		lifecycle.BeginLoad();
	}

	void GameStarted()
	{
		const std::scoped_lock lock(mutex);
		lifecycle.GameStarted();
	}

	bool TryStartSession()
	{
		const auto playerInCell = PlayerInCell();
		const auto blockingMenuOpen = BlockingMenuOpen();
		{
			const std::scoped_lock lock(mutex);
			if (!lifecycle.TryActivate(playerInCell, blockingMenuOpen))
				return false;
		}
		BridgeController::GetSingleton().StartSession("Active game detected without a game-start notification");
		return true;
	}

	bool CanUseGame()
	{
		const auto             playerInCell = PlayerInCell();
		const auto             blockingMenuOpen = BlockingMenuOpen();
		const std::scoped_lock lock(mutex);
		return lifecycle.CanUseGame(playerInCell, blockingMenuOpen);
	}
}
