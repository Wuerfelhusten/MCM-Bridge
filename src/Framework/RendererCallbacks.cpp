#include "MCMBridge/Framework/RendererCallbacks.h"

#include <Windows.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace
{
	constexpr std::size_t slotsPerBlock = 128;
	constexpr std::size_t stride = 32;
	constexpr std::size_t blockBytes = slotsPerBlock * stride;
	static_assert(sizeof(std::uintptr_t) == 8);

	void* CreateBlock(std::size_t a_base, MCMBridge::RendererCallbacks::Receiver a_receiver)
	{
		auto* block = static_cast<std::byte*>(VirtualAlloc(nullptr, blockBytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
		if (!block)
			throw std::bad_alloc();
		for (std::size_t index = 0; index < slotsPerBlock; ++index) {
			// ENDBR64; mov rcx, slot; mov rax, receiver; jmp rax.
			std::array<std::uint8_t, stride> code{ 0xF3, 0x0F, 0x1E, 0xFA, 0x48, 0xB9 };
			const auto                       slot = static_cast<std::uint64_t>(a_base + index);
			const auto                       address = reinterpret_cast<std::uintptr_t>(a_receiver);
			std::memcpy(code.data() + 6, &slot, sizeof(slot));
			code[14] = 0x48;
			code[15] = 0xB8;
			std::memcpy(code.data() + 16, &address, sizeof(address));
			code[24] = 0xFF;
			code[25] = 0xE0;
			std::memcpy(block + index * stride, code.data(), code.size());
		}
		DWORD previous{};
		if (!VirtualProtect(block, blockBytes, PAGE_EXECUTE_READ, &previous) ||
			!FlushInstructionCache(GetCurrentProcess(), block, blockBytes)) {
			VirtualFree(block, 0, MEM_RELEASE);
			throw std::runtime_error("Could not publish Framework renderer callbacks");
		}
		return block;
	}
}

namespace MCMBridge
{
	RendererCallbacks::~RendererCallbacks()
	{
		for (auto* block : blocks)
			VirtualFree(block, 0, MEM_RELEASE);
	}

	RendererCallbacks::Callback RendererCallbacks::Get(std::size_t a_slot)
	{
		if (!receiver || a_slot > std::numeric_limits<std::size_t>::max() - slotsPerBlock)
			throw std::invalid_argument("Invalid Framework renderer slot");
		const auto target = a_slot / slotsPerBlock;
		while (blocks.size() <= target) {
			blocks.reserve(blocks.size() + 1);
			blocks.push_back(CreateBlock(blocks.size() * slotsPerBlock, receiver));
		}
		return reinterpret_cast<Callback>(static_cast<std::byte*>(blocks[target]) + (a_slot % slotsPerBlock) * stride);
	}
}
