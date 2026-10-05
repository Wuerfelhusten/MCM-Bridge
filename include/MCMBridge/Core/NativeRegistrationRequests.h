#pragma once

#include <cstdint>
#include <mutex>
#include <unordered_map>

namespace MCMBridge
{
	// Requests own no VM stack. A restored script can observe a missing request
	// and finish normally; owner checks prevent consuming another stack's result.
	class NativeRegistrationRequests
	{
	public:
		static constexpr std::int32_t kPending = -3;
		void                          Reset(std::uint64_t a_session);
		std::int32_t                  Submit(std::uint64_t a_session, std::uint32_t a_owner);
		bool                          Claim(std::uint64_t a_session, std::int32_t a_request);
		bool                          Complete(std::uint64_t a_session, std::int32_t a_request, std::int32_t a_result);
		std::int32_t                  Take(std::uint64_t a_session, std::uint32_t a_owner, std::int32_t a_request);
		void                          Cancel(std::uint64_t a_session, std::uint32_t a_owner, std::int32_t a_request);

	private:
		struct Entry
		{
			std::uint32_t owner;
			std::int32_t  result{ kPending };
			bool          claimed{};
		};
		std::mutex                              mutex;
		std::uint64_t                           session{};
		std::int32_t                            nextRequest{};
		std::unordered_map<std::int32_t, Entry> entries;
	};
}
