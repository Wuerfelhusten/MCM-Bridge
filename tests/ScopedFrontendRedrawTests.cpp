#include "MCMBridge/Core/ScopedFrontendRedraw.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Only an authenticated synchronous redraw avoids global invalidation")
{
	using MCMBridge::ScopedFrontendRedraw;
	constexpr auto menu = "Journal Menu";
	constexpr auto target = "_root.ConfigPanelFader.configPanel.invalidateOptionData";
	REQUIRE_FALSE(ScopedFrontendRedraw::Owns(menu, target));
	{
		const ScopedFrontendRedraw scope(true, menu, target);
		REQUIRE(ScopedFrontendRedraw::Owns(menu, target));
		REQUIRE_FALSE(ScopedFrontendRedraw::Owns("Other Menu", target));
		REQUIRE_FALSE(ScopedFrontendRedraw::Owns(menu, "_root.other.invalidateOptionData"));
		{
			const ScopedFrontendRedraw nested(false, menu, target);
			REQUIRE_FALSE(ScopedFrontendRedraw::Owns(menu, target));
		}
		REQUIRE(ScopedFrontendRedraw::Owns(menu, target));
	}
	REQUIRE_FALSE(ScopedFrontendRedraw::Owns(menu, target));
	const ScopedFrontendRedraw reset(true, menu, "_root.ConfigPanelFader.configPanel.forcePageReset");
	REQUIRE_FALSE(ScopedFrontendRedraw::Owns(menu, "_root.ConfigPanelFader.configPanel.forcePageReset"));
}
