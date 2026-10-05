#include "MCMBridge/Core/SessionLifecycle.h"

#include <catch2/catch_test_macros.hpp>

#include <functional>
#include <vector>

TEST_CASE("An active game bootstraps once without an SKSE new game message")
{
	MCMBridge::SessionLifecycle lifecycle;
	CHECK(lifecycle.NeedsActivation());
	CHECK(lifecycle.TryActivate(true, false));
	CHECK_FALSE(lifecycle.NeedsActivation());
	CHECK_FALSE(lifecycle.TryActivate(true, false));
}

TEST_CASE("Session bootstrap waits for a player cell and closed startup menus")
{
	MCMBridge::SessionLifecycle lifecycle;
	CHECK_FALSE(lifecycle.TryActivate(false, false));
	CHECK_FALSE(lifecycle.TryActivate(true, true));
	CHECK_FALSE(lifecycle.TryActivate(false, true));
	CHECK(lifecycle.NeedsActivation());
	CHECK(lifecycle.TryActivate(true, false));
}

TEST_CASE("A queued startup menu event cannot clear the game after a direct world start")
{
	MCMBridge::SessionLifecycle        lifecycle;
	bool                               mainMenuOpen = true;
	bool                               listPopulated{};
	std::vector<std::function<void()>> tasks;
	const auto                         token = lifecycle.ObserveMainMenu(true);
	tasks.push_back([&, token] {
		if (lifecycle.IsCurrentMainMenuEvent(token, mainMenuOpen))
			listPopulated = false;
	});
	mainMenuOpen = false;
	lifecycle.ObserveMainMenu(false);
	REQUIRE(lifecycle.TryActivate(true, false));
	listPopulated = true;
	for (const auto& task : tasks) task();
	CHECK(listPopulated);
	CHECK_FALSE(lifecycle.NeedsActivation());
}

TEST_CASE("Queued main menu resets also verify the current engine menu state")
{
	MCMBridge::SessionLifecycle lifecycle;
	const auto                  token = lifecycle.ObserveMainMenu(true);
	CHECK(lifecycle.IsCurrentMainMenuEvent(token, true));
	CHECK_FALSE(lifecycle.IsCurrentMainMenuEvent(token, false));
}

TEST_CASE("A later main menu event invalidates the previously queued reset")
{
	MCMBridge::SessionLifecycle lifecycle;
	const auto                  old = lifecycle.ObserveMainMenu(true);
	lifecycle.ObserveMainMenu(false);
	const auto current = lifecycle.ObserveMainMenu(true);
	CHECK_FALSE(lifecycle.IsCurrentMainMenuEvent(old, true));
	CHECK(lifecycle.IsCurrentMainMenuEvent(current, true));
}

TEST_CASE("Official game start notifications invalidate earlier menu resets")
{
	MCMBridge::SessionLifecycle lifecycle;
	const auto                  old = lifecycle.ObserveMainMenu(true);
	lifecycle.GameStarted();
	CHECK_FALSE(lifecycle.IsCurrentMainMenuEvent(old, true));
	CHECK_FALSE(lifecycle.TryActivate(true, false));
	CHECK(lifecycle.CanUseGame(true, false));
}

TEST_CASE("A load in progress cannot bootstrap from the previous player cell")
{
	MCMBridge::SessionLifecycle lifecycle;
	lifecycle.GameStarted();
	const auto old = lifecycle.ObserveMainMenu(true);
	lifecycle.BeginLoad();
	CHECK_FALSE(lifecycle.IsCurrentMainMenuEvent(old, true));
	CHECK_FALSE(lifecycle.CanUseGame(true, false));
	CHECK_FALSE(lifecycle.TryActivate(true, false));
	const auto duringLoad = lifecycle.ObserveMainMenu(true);
	CHECK_FALSE(lifecycle.IsCurrentMainMenuEvent(duringLoad, true));
	lifecycle.GameStarted();
	CHECK_FALSE(lifecycle.IsCurrentMainMenuEvent(duringLoad, true));
	CHECK(lifecycle.CanUseGame(true, false));
}

TEST_CASE("An active game cannot execute MCM calls during a blocking menu")
{
	MCMBridge::SessionLifecycle lifecycle;
	lifecycle.GameStarted();
	CHECK(lifecycle.CanUseGame(true, false));
	CHECK_FALSE(lifecycle.CanUseGame(true, true));
	CHECK(lifecycle.CanUseGame(true, false));
	lifecycle.BeginLoad();
	CHECK_FALSE(lifecycle.CanUseGame(true, false));
	lifecycle.GameStarted();
	CHECK_FALSE(lifecycle.CanUseGame(true, true));
	CHECK(lifecycle.CanUseGame(true, false));
}

TEST_CASE("Returning to the menu rearms fallback for a second direct world start")
{
	MCMBridge::SessionLifecycle lifecycle;
	lifecycle.GameStarted();
	lifecycle.ObserveMainMenu(true);
	CHECK(lifecycle.NeedsActivation());
	CHECK_FALSE(lifecycle.TryActivate(true, true));
	lifecycle.ObserveMainMenu(false);
	CHECK(lifecycle.TryActivate(true, false));
	CHECK_FALSE(lifecycle.TryActivate(true, false));
}
