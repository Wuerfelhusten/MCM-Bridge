#pragma once

#include <memory>

namespace MCMBridge
{
	// Verified Windows x64 Helper 1.4.0/1.5.0 await-once contract, not a general coroutine.
	// The game-queue owner retains this object until Complete resumes the consumer.
	class HelperMessageTask
	{
	public:
		HelperMessageTask();
		~HelperMessageTask();
		HelperMessageTask(const HelperMessageTask&) = delete;
		HelperMessageTask& operator=(const HelperMessageTask&) = delete;
		void*              Address() const;
		// False means the foreign consumer has not attached yet. Retry on the game queue.
		bool         Complete(bool a_result);
		void         Abandon();
		bool         Cancelled() const;
		static void* Rejected();

	private:
		struct Frame;
		std::unique_ptr<Frame> frame;
	};
}
