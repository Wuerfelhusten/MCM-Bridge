#pragma once

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace MCMBridge
{
	constexpr bool EqualPapyrusTypeName(std::string_view a_left, std::string_view a_right)
	{
		if (a_left.size() != a_right.size())
			return false;
		const auto fold = [](char a_value) { return a_value >= 'A' && a_value <= 'Z' ? a_value + ('a' - 'A') : a_value; };
		for (std::size_t index = 0; index < a_left.size(); ++index)
			if (fold(a_left[index]) != fold(a_right[index]))
				return false;
		return true;
	}

	// Diagnostic admission only. Registry notifications drive retries, never a timer.
	class NativePreflightGate
	{
	public:
		bool ShouldRun(std::uint64_t a_session, std::vector<std::string> a_identities)
		{
			if (!a_session || a_identities.empty())
				return false;
			std::ranges::sort(a_identities);
			if (session == a_session && identities == a_identities)
				return false;
			session = a_session;
			identities = std::move(a_identities);
			return true;
		}

	private:
		std::uint64_t            session{};
		std::vector<std::string> identities;
	};
}
