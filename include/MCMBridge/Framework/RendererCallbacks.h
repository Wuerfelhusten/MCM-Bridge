#pragma once

#include <cstddef>
#include <vector>

namespace MCMBridge
{
	// The Framework API has no user-data argument. Immutable x64 thunks bind
	// each slot to one callback. Their owner must outlive registered entries.
	class RendererCallbacks
	{
	public:
		using Callback = void (*)();
		using Receiver = void (*)(std::size_t);
		explicit RendererCallbacks(Receiver a_receiver) : receiver(a_receiver) {}
		~RendererCallbacks();
		RendererCallbacks(const RendererCallbacks&) = delete;
		RendererCallbacks& operator=(const RendererCallbacks&) = delete;
		Callback           Get(std::size_t a_slot);

	private:
		Receiver           receiver;
		std::vector<void*> blocks;
	};
}
