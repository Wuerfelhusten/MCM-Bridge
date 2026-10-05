#pragma once

#include <atomic>
#include <cstdint>
#include <string>

namespace MCMBridge
{
	struct OriginalPageTarget
	{
		std::int32_t configIndex{ -1 };
		std::string  pageName;
		std::int32_t pageIndex{ -1 };
		std::string  registryID;
	};

	class OriginalPageNavigator
	{
	public:
		static OriginalPageNavigator& GetSingleton();

		void Open(OriginalPageTarget a_target);
		void Cancel();

	private:
		enum class Phase
		{
			kOpenPanel,
			kSelectMod,
			kSelectPage
		};

		void Advance(std::uint64_t a_generation, OriginalPageTarget a_target, Phase a_phase, std::uint32_t a_attempt);
		void Schedule(std::uint64_t a_generation, OriginalPageTarget a_target, Phase a_phase, std::uint32_t a_attempt);

		std::atomic_uint64_t generation{};
	};
}
