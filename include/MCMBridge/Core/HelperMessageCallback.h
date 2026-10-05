#pragma once

#include <array>
#include <cstddef>

namespace MCMBridge
{
	// Only for the verified 1.6.2/1.6.3 Windows x64 release callback ABI.
	// Copy and destruction stay in the foreign module, including its allocator.
	class HelperMessageCallback
	{
	public:
		explicit HelperMessageCallback(void* a_storage);
		~HelperMessageCallback();
		HelperMessageCallback(const HelperMessageCallback&) = delete;
		HelperMessageCallback& operator=(const HelperMessageCallback&) = delete;
		void                   Complete(bool a_result);
		void                   Abandon();
		static void            DestroyArgument(void* a_storage);

	private:
		alignas(16) std::array<std::byte, 64> storage{};
		void* callable{};
	};
}
