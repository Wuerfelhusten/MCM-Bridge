#include "MCMBridge/Core/HelperMessageCallback.h"

#include <array>
#include <catch2/catch_test_macros.hpp>
#include <functional>
#include <memory>
#include <stdexcept>

static_assert(sizeof(std::function<void(bool)>) == 64);

TEST_CASE("Helper callback tolerates reentrant completion", "[helper][message]")
{
	int                               calls{};
	MCMBridge::HelperMessageCallback* active{};
	std::function<void(bool)>         source = [&](bool) {
		++calls;
		active->Complete(false);
	};
	MCMBridge::HelperMessageCallback callback(&source);
	active = &callback;
	MCMBridge::HelperMessageCallback::DestroyArgument(&source);
	callback.Complete(true);
	REQUIRE(calls == 1);
}

TEST_CASE("Helper callback copies small callable and consumes its argument exactly once", "[helper][message]")
{
	int                              calls{};
	bool                             answer{};
	std::function<void(bool)>        source = [&](bool a_answer) { ++calls; answer = a_answer; };
	MCMBridge::HelperMessageCallback callback(&source);
	MCMBridge::HelperMessageCallback::DestroyArgument(&source);
	REQUIRE_FALSE(source);
	callback.Complete(true);
	callback.Complete(false);
	callback.Abandon();
	REQUIRE(calls == 1);
	REQUIRE(answer);
}

TEST_CASE("Helper callback keeps heap callable alive and releases it through foreign destruction", "[helper][message]")
{
	auto                             lifetime = std::make_shared<int>(0);
	std::weak_ptr<int>               weak = lifetime;
	std::array<int, 128>             payload{};
	std::function<void(bool)>        source = [lifetime, payload](bool a_answer) { *lifetime = a_answer ? payload[0] : 7; };
	MCMBridge::HelperMessageCallback callback(&source);
	MCMBridge::HelperMessageCallback::DestroyArgument(&source);
	callback.Complete(false);
	REQUIRE(*lifetime == 7);
	lifetime.reset();
	REQUIRE(weak.expired());
}

TEST_CASE("Helper callback abandonment never invokes an expired session", "[helper][message]")
{
	int                              calls{};
	std::function<void(bool)>        source = [&](bool) { ++calls; };
	MCMBridge::HelperMessageCallback callback(&source);
	MCMBridge::HelperMessageCallback::DestroyArgument(&source);
	callback.Abandon();
	callback.Complete(true);
	REQUIRE(calls == 0);
}

TEST_CASE("Helper empty callback and throwing callback release ownership", "[helper][message]")
{
	std::function<void(bool)>        empty;
	MCMBridge::HelperMessageCallback noCallback(&empty);
	noCallback.Complete(false);
	MCMBridge::HelperMessageCallback::DestroyArgument(&empty);
	auto                             lifetime = std::make_shared<int>(0);
	std::weak_ptr<int>               weak = lifetime;
	std::function<void(bool)>        source = [lifetime](bool) { throw std::runtime_error("callback failure"); };
	MCMBridge::HelperMessageCallback callback(&source);
	MCMBridge::HelperMessageCallback::DestroyArgument(&source);
	lifetime.reset();
	REQUIRE_THROWS_AS(callback.Complete(false), std::runtime_error);
	REQUIRE(weak.expired());
	callback.Complete(true);
}
