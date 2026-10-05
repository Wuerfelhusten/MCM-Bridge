#include "MCMBridge/Framework/RendererCallbacks.h"

#include <Windows.h>
#include <catch2/catch_test_macros.hpp>

#include <limits>
#include <vector>

namespace
{
	std::size_t rendered{};
	void        Render(std::size_t a_slot) { rendered = a_slot; }
}

TEST_CASE("Framework callbacks retain unique page slots beyond the former 1024 limit")
{
	MCMBridge::RendererCallbacks                        callbacks(Render);
	std::vector<MCMBridge::RendererCallbacks::Callback> functions;
	for (std::size_t slot = 0; slot < 4097; ++slot)
		functions.push_back(callbacks.Get(slot));
	for (std::size_t slot = functions.size(); slot-- > 0;) {
		rendered = std::numeric_limits<std::size_t>::max();
		functions[slot]();
		CHECK(rendered == slot);
		CHECK(callbacks.Get(slot) == functions[slot]);
	}
	CHECK_THROWS(callbacks.Get(std::numeric_limits<std::size_t>::max()));
	MEMORY_BASIC_INFORMATION memory{};
	REQUIRE(VirtualQuery(reinterpret_cast<const void*>(functions.front()), &memory, sizeof(memory)) == sizeof(memory));
	CHECK(memory.Protect == PAGE_EXECUTE_READ);
}
