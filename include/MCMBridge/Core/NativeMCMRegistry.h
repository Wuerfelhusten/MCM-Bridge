#pragma once

#include "MCMBridge/Core/Result.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace MCMBridge
{
	struct NativeMCMEntry
	{
		std::uint64_t instance{};
		std::string   identity;
		std::string   name;
	};

	struct NativeRegistryView
	{
		std::uint64_t session{};
		std::uint64_t revision{};
		std::size_t   count{};
		// Slots are also the compatibility _configID indices. Never compact a live session.
		std::vector<std::optional<NativeMCMEntry>> slots;
	};

	// The adapter owns VM references; instance is a session-local opaque token, not a form ID.
	class NativeMCMRegistry
	{
	public:
		std::uint64_t        BeginSession();
		Result<std::int32_t> Register(std::uint64_t a_session, NativeMCMEntry a_entry);
		// Entries retain submission order. Failure publishes no registrations or renames.
		Result<std::vector<std::int32_t>> RegisterBatch(std::uint64_t a_session, std::vector<NativeMCMEntry> a_entries);
		Result<bool>                      Unregister(std::uint64_t a_session, std::uint64_t a_instance);
		// Bootstrap saved compatibility slots without changing existing _configID values.
		Result<bool>                  Import(std::uint64_t a_session, std::vector<std::optional<NativeMCMEntry>> a_slots);
		NativeRegistryView            Read() const;
		std::uint64_t                 Revision() const;
		Result<std::size_t>           Count(std::uint64_t a_session) const;
		std::optional<NativeMCMEntry> Find(std::uint64_t a_session, std::int32_t a_slot) const;
		std::optional<std::int32_t>   FindSlot(std::uint64_t a_session, std::uint64_t a_instance) const;

	private:
		mutable std::mutex                              mutex;
		NativeRegistryView                              state;
		std::unordered_map<std::uint64_t, std::int32_t> instances;
		std::unordered_map<std::string, std::uint64_t>  identities;
	};
}
