#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace MCMBridge
{
	enum class FacadeCallKind
	{
		kOpen,
		kPage,
		kOperation,
		kClose
	};

	// Game-queue owned. A nested call may reuse only its originating VM stack.
	// Permits never repeat, including after a save change or a timed-out call.
	class FacadeCallState
	{
	public:
		bool                         Open(std::uint64_t a_session, std::uint64_t a_owner, std::int32_t a_token);
		bool                         Adopt(std::uint64_t a_session, std::uint64_t a_owner, std::int32_t a_token);
		std::optional<std::uint64_t> Enter(std::uint64_t a_session, std::uint64_t a_owner,
			std::uint32_t a_stack, FacadeCallKind a_kind);
		bool                         Leave(std::uint64_t a_permit, std::uint32_t a_stack);
		bool                         Owns(std::uint64_t a_session, std::uint64_t a_owner) const;
		bool                         Running(std::uint64_t a_permit) const;
		bool                         Busy() const { return !calls.empty(); }
		bool                         IsOpen() const { return opened; }
		std::int32_t                 Token() const { return token; }
		void                         Reset();

	private:
		struct Call
		{
			std::uint64_t  permit;
			FacadeCallKind kind;
		};
		std::uint64_t     session{};
		std::uint64_t     owner{};
		std::int32_t      token{};
		std::uint32_t     stack{};
		std::uint64_t     nextPermit{ 1 };
		bool              opened{};
		std::vector<Call> calls;
	};
}
