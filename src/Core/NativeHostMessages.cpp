#include "MCMBridge/Core/NativeHostSession.h"

#include <limits>

namespace MCMBridge
{
	std::int32_t NativeHostSession::BeginMessage(std::int32_t a_token)
	{
		const std::scoped_lock lock(mutex);
		if (!Owns(a_token) || messageRequest || nextMessage == std::numeric_limits<std::int32_t>::max())
			return 0;
		messageResult.reset();
		messageStarted = clock.Now();
		messageRequest = ++nextMessage;
		return messageRequest;
	}

	bool NativeHostSession::IsMessageActive(std::int32_t a_token, std::int32_t a_request) const
	{
		const std::scoped_lock lock(mutex);
		return Owns(a_token) && a_request > 0 && messageRequest == a_request && !messageResult;
	}

	bool NativeHostSession::CompleteMessage(std::int32_t a_token, std::int32_t a_request, bool a_accepted)
	{
		const std::scoped_lock lock(mutex);
		if (!Owns(a_token) || a_request <= 0 || messageRequest != a_request || messageResult)
			return false;
		messageResult = a_accepted;
		messageWait += clock.Now() - messageStarted;
		return true;
	}

	std::int32_t NativeHostSession::TakeMessage(std::int32_t a_token, std::int32_t a_request)
	{
		const std::scoped_lock lock(mutex);
		if (!Owns(a_token) || a_request <= 0 || messageRequest != a_request)
			return -2;
		if (!messageResult)
			return -1;
		const auto result = *messageResult ? 1 : 0;
		messageRequest = 0;
		messageResult.reset();
		return result;
	}

	std::chrono::steady_clock::duration NativeHostSession::MessageWaitDuration(std::int32_t a_token) const
	{
		const std::scoped_lock lock(mutex);
		if (!Owns(a_token))
			return {};
		return messageWait + (messageRequest && !messageResult ? clock.Now() - messageStarted : std::chrono::steady_clock::duration{});
	}
}
