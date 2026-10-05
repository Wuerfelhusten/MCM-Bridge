#include "MCMBridge/Core/FacadeCallState.h"

#include <algorithm>

namespace MCMBridge
{
	bool FacadeCallState::Adopt(std::uint64_t a_session, std::uint64_t a_owner, std::int32_t a_token)
	{
		if (!Open(a_session, a_owner, a_token))
			return false;
		opened = true;
		return true;
	}

	bool FacadeCallState::Open(std::uint64_t a_session, std::uint64_t a_owner, std::int32_t a_token)
	{
		if (token || !a_session || !a_owner || a_token <= 0)
			return false;
		session = a_session;
		owner = a_owner;
		token = a_token;
		return true;
	}

	bool FacadeCallState::Owns(std::uint64_t a_session, std::uint64_t a_owner) const
	{
		return token > 0 && session == a_session && owner == a_owner;
	}

	std::optional<std::uint64_t> FacadeCallState::Enter(std::uint64_t a_session, std::uint64_t a_owner,
		std::uint32_t a_stack, FacadeCallKind a_kind)
	{
		if (!Owns(a_session, a_owner) || !nextPermit || calls.size() >= 64)
			return std::nullopt;
		if (!calls.empty() && (stack != a_stack || a_kind == FacadeCallKind::kOpen || a_kind == FacadeCallKind::kClose || calls.back().kind == FacadeCallKind::kClose))
			return std::nullopt;
		if (calls.empty() && ((a_kind == FacadeCallKind::kOpen) == opened))
			return std::nullopt;
		const auto permit = nextPermit++;
		calls.push_back({ permit, a_kind });
		stack = a_stack;
		return permit;
	}

	bool FacadeCallState::Leave(std::uint64_t a_permit, std::uint32_t a_stack)
	{
		if (calls.empty() || stack != a_stack || calls.back().permit != a_permit)
			return false;
		const auto kind = calls.back().kind;
		calls.pop_back();
		if (kind == FacadeCallKind::kOpen)
			opened = true;
		else if (kind == FacadeCallKind::kClose)
			Reset();
		return true;
	}

	bool FacadeCallState::Running(std::uint64_t a_permit) const
	{
		return std::ranges::any_of(calls, [a_permit](const Call& a_call) { return a_call.permit == a_permit; });
	}

	void FacadeCallState::Reset()
	{
		calls.clear();
		session = owner = 0;
		token = 0;
		stack = 0;
		opened = false;
	}
}
