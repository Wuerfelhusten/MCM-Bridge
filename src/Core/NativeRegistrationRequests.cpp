#include "MCMBridge/Core/NativeRegistrationRequests.h"

#include <limits>

namespace MCMBridge
{
	void NativeRegistrationRequests::Reset(std::uint64_t a_session)
	{
		const std::scoped_lock lock(mutex);
		session = a_session;
		entries.clear();
	}

	std::int32_t NativeRegistrationRequests::Submit(std::uint64_t a_session, std::uint32_t a_owner)
	{
		const std::scoped_lock lock(mutex);
		if (!session || a_session != session || nextRequest == std::numeric_limits<std::int32_t>::max())
			return -1;
		const auto request = ++nextRequest;
		entries.emplace(request, Entry{ a_owner });
		return request;
	}

	bool NativeRegistrationRequests::Claim(std::uint64_t a_session, std::int32_t a_request)
	{
		const std::scoped_lock lock(mutex);
		const auto             found = entries.find(a_request);
		if (a_session != session || found == entries.end() || found->second.claimed)
			return false;
		found->second.claimed = true;
		return true;
	}

	bool NativeRegistrationRequests::Complete(std::uint64_t a_session, std::int32_t a_request, std::int32_t a_result)
	{
		const std::scoped_lock lock(mutex);
		const auto             found = entries.find(a_request);
		if (a_session != session || found == entries.end() || !found->second.claimed || found->second.result != kPending || a_result == kPending)
			return false;
		found->second.result = a_result;
		return true;
	}

	std::int32_t NativeRegistrationRequests::Take(std::uint64_t a_session, std::uint32_t a_owner, std::int32_t a_request)
	{
		const std::scoped_lock lock(mutex);
		const auto             found = entries.find(a_request);
		if (a_session != session || found == entries.end() || found->second.owner != a_owner)
			return -1;
		const auto result = found->second.result;
		if (result != kPending)
			entries.erase(found);
		return result;
	}

	void NativeRegistrationRequests::Cancel(std::uint64_t a_session, std::uint32_t a_owner, std::int32_t a_request)
	{
		const std::scoped_lock lock(mutex);
		const auto             found = entries.find(a_request);
		if (a_session == session && found != entries.end() && found->second.owner == a_owner)
			entries.erase(found);
	}
}
