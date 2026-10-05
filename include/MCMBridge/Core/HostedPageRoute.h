#pragma once

#include "MCMBridge/Core/ViewLoadState.h"

#include <string>
#include <string_view>

namespace MCMBridge
{
	// The controller serializes access with its hosted request mutex.
	class HostedPageRoute
	{
	public:
		void Clear()
		{
			mod.clear();
			origin.clear();
			target.clear();
		}
		void Redirect(std::string_view a_mod, std::string_view a_origin, std::string_view a_target)
		{
			if (mod != a_mod || target != a_origin)
				origin = a_origin;
			mod = a_mod;
			target = a_target;
		}
		void Close(std::string_view a_mod, std::string_view a_page)
		{
			Redirect(a_mod, a_page, {});
		}
		bool Remap(ViewLoadState& a_view, std::uint64_t a_revision, std::string_view a_mod,
			std::string_view a_origin, std::string_view a_target)
		{
			if (!a_view.Remap(a_revision, a_target))
				return false;
			Redirect(a_mod, a_origin, a_target);
			return true;
		}
		std::string Resolve(std::string_view a_mod, std::string_view a_page)
		{
			if (mod == a_mod && (origin == a_page || target == a_page))
				return target;
			Clear();
			return std::string(a_page);
		}

	private:
		std::string mod;
		std::string origin;
		std::string target;
	};
}
