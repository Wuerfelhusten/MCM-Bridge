#pragma once

#include <string_view>

namespace MCMBridge
{
	// Permission lasts only through synchronous forwarding of an authenticated call.
	class ScopedFrontendRedraw
	{
	public:
		ScopedFrontendRedraw(bool a_authenticated, std::string_view a_menu, std::string_view a_target) :
			authenticated(a_authenticated), menu(a_menu), target(a_target), previous(current)
		{
			current = this;
		}
		~ScopedFrontendRedraw() { current = previous; }
		ScopedFrontendRedraw(const ScopedFrontendRedraw&) = delete;
		ScopedFrontendRedraw& operator=(const ScopedFrontendRedraw&) = delete;
		static bool           Owns(std::string_view a_menu, std::string_view a_target)
		{
			return current && current->authenticated && current->menu == a_menu && current->target == a_target &&
			       a_target.ends_with(".invalidateOptionData");
		}

	private:
		bool                                                   authenticated{};
		std::string_view                                       menu;
		std::string_view                                       target;
		const ScopedFrontendRedraw*                            previous{};
		inline static thread_local const ScopedFrontendRedraw* current{};
	};
}
