#pragma once

#include "MCMBridge/Core/OperationTimer.h"
#include "MCMBridge/Papyrus/IClassicScript.h"

#include <memory>

namespace MCMBridge
{
	enum class HostCallStatus
	{
		kCompleted,
		kDispatchFailed,
		kInvalidData,
		kControlUnavailable,
		kCancelled,
		kTimedOut,
		kSessionInvalidated
	};

	// Game-task owned. The adapter must deliver completion on the same queue,
	// after publishing its page and compatibility buffers. No VM work is retried.
	class HostCallSession final : public std::enable_shared_from_this<HostCallSession>
	{
	public:
		using Completion = std::function<void(HostCallStatus)>;

		HostCallSession(std::shared_ptr<IClassicScript> a_script, IOperationTimer& a_timer);
		~HostCallSession();
		// A false return rejects submission without invoking the completion.
		// Every accepted call completes once, including cancellation and expiry.
		bool Submit(ClassicCall a_call, Completion a_completion, std::chrono::milliseconds a_timeout);
		void Cancel();
		bool Close(Completion a_completion, std::chrono::milliseconds a_timeout);
		void Invalidate();
		bool Busy() const { return static_cast<bool>(completion); }
		bool Stopped() const { return stopped; }

	private:
		void Complete(std::uint64_t a_call);
		void Expire(std::uint64_t a_call);
		void Finish(HostCallStatus a_status);
		void Retire(HostCallStatus a_status);
		bool Current(std::uint64_t a_call);
		void ReopenPage(std::uint64_t a_call);

		std::shared_ptr<IClassicScript>     script;
		OperationDeadline                   deadline;
		std::optional<MCMControl>           transitionControl;
		std::optional<ClassicPageSelection> transitionPage;
		std::int32_t                        transitionIndex{};
		bool                                reopened{};
		ClassicCall                         call;
		Completion                          completion;
		std::uint64_t                       serial{};
		bool                                stopped{};
	};
}
