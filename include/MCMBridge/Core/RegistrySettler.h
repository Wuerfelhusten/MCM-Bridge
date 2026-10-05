#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace MCMBridge
{
	enum class RegistrySettleResult
	{
		kEmpty,
		kChanged,
		kWaiting,
		kReady,
		kExpired
	};

	class RegistrySettler
	{
	public:
		RegistrySettleResult Observe(std::vector<std::string> a_ids);
		void                 Reset();
		bool                 ShouldContinue() const;

	private:
		static constexpr std::uint32_t maximumChecks = 30;
		static constexpr std::uint32_t requiredQuietChecks = 2;

		std::vector<std::string> lastIDs;
		std::uint32_t            checks{};
		std::uint32_t            quietChecks{};
	};
}
