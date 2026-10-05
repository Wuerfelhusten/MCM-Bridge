#include "MCMBridge/Core/HelperMessageTask.h"

#include <atomic>
#include <coroutine>
#include <cstddef>
#include <exception>

namespace MCMBridge
{
	struct HelperMessageTask::Frame
	{
		static void Resume(Frame*) noexcept {}
		static void Destroy(Frame* a_frame) noexcept { a_frame->cancelled.store(true); }
		static void DestroyRejected(Frame*) noexcept {}

		void (*resume)(Frame*) = Resume;
		void (*destroy)(Frame*) = Destroy;
		std::exception_ptr      exception;
		std::coroutine_handle<> continuation;
		bool                    value{};
		std::atomic<bool>       cancelled{};
		bool                    completed{};
	};

	HelperMessageTask::HelperMessageTask() : frame(std::make_unique<Frame>())
	{
		// Verified against the actual consumer instructions, not just the source type.
		static_assert(sizeof(void*) == 8 && sizeof(std::exception_ptr) == 16);
		static_assert(offsetof(Frame, exception) == 0x10);
		static_assert(offsetof(Frame, continuation) == 0x20);
		static_assert(offsetof(Frame, value) == 0x28);
	}
	HelperMessageTask::~HelperMessageTask() = default;
	void* HelperMessageTask::Address() const { return frame.get(); }
	bool  HelperMessageTask::Cancelled() const { return frame->cancelled.load(); }
	void  HelperMessageTask::Abandon() { frame->cancelled.store(true); }

	bool HelperMessageTask::Complete(bool a_result)
	{
		if (frame->completed || Cancelled())
			return true;
		// The verified foreign await_suspend publishes one aligned x64 pointer store.
		// Do not complete before that store, including an immediately rejected dialog.
		const auto continuation = std::atomic_ref(frame->continuation).load(std::memory_order_acquire);
		if (!continuation)
			return false;
		frame->value = a_result;
		frame->completed = true;
		frame->resume = nullptr;
		continuation.resume();
		return true;
	}

	void* HelperMessageTask::Rejected()
	{
		static Frame      rejected;
		static const bool initialized = [] {
			rejected.resume = nullptr;
			rejected.destroy = Frame::DestroyRejected;
			return true;
		}();
		(void)initialized;
		return &rejected;
	}
}
