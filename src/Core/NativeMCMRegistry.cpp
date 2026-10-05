#include "MCMBridge/Core/NativeMCMRegistry.h"

#include <limits>
#include <stdexcept>

namespace MCMBridge
{
	std::uint64_t NativeMCMRegistry::BeginSession()
	{
		const std::scoped_lock lock(mutex);
		if (state.session == std::numeric_limits<std::uint64_t>::max())
			throw std::overflow_error("Native MCM session tokens exhausted");
		state = { .session = state.session + 1, .revision = 1 };
		instances.clear();
		identities.clear();
		return state.session;
	}

	Result<std::int32_t> NativeMCMRegistry::Register(std::uint64_t a_session, NativeMCMEntry a_entry)
	{
		const std::scoped_lock lock(mutex);
		if (!a_session || a_session != state.session)
			return std::unexpected(BridgeError{ BridgeErrorCode::kStaleSnapshot, "Native MCM registry session changed" });
		if (!a_entry.instance || a_entry.identity.empty())
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Native MCM registration has no identity" });
		if (const auto found = instances.find(a_entry.instance); found != instances.end()) {
			auto& current = *state.slots[static_cast<std::size_t>(found->second)];
			if (current.identity != a_entry.identity)
				return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Native MCM instance identity changed" });
			if (current.name != a_entry.name) {
				current.name = std::move(a_entry.name);
				++state.revision;
			}
			return found->second;
		}
		if (identities.contains(a_entry.identity))
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Native MCM identity is already bound to another instance" });
		if (state.slots.size() >= static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
			return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "Native MCM compatibility indices exhausted" });
		const auto slot = static_cast<std::int32_t>(state.slots.size());
		// Allocate the slot before indexing; roll back all partial insertions on allocation failure.
		state.slots.emplace_back(std::move(a_entry));
		const auto& entry = *state.slots.back();
		try {
			identities.emplace(entry.identity, entry.instance);
			instances.emplace(entry.instance, slot);
		} catch (...) {
			identities.erase(entry.identity);
			instances.erase(entry.instance);
			state.slots.pop_back();
			throw;
		}
		++state.count;
		++state.revision;
		return slot;
	}

	Result<bool> NativeMCMRegistry::Unregister(std::uint64_t a_session, std::uint64_t a_instance)
	{
		const std::scoped_lock lock(mutex);
		if (!a_session || a_session != state.session)
			return std::unexpected(BridgeError{ BridgeErrorCode::kStaleSnapshot, "Native MCM registry session changed" });
		const auto found = instances.find(a_instance);
		if (found == instances.end())
			return false;
		auto& entry = state.slots[static_cast<std::size_t>(found->second)];
		identities.erase(entry->identity);
		entry.reset();
		instances.erase(found);
		--state.count;
		++state.revision;
		return true;
	}

	std::optional<std::int32_t> NativeMCMRegistry::FindSlot(std::uint64_t a_session, std::uint64_t a_instance) const
	{
		const std::scoped_lock lock(mutex);
		if (!a_session || a_session != state.session)
			return std::nullopt;
		const auto found = instances.find(a_instance);
		return found == instances.end() ? std::nullopt : std::optional(found->second);
	}

	NativeRegistryView NativeMCMRegistry::Read() const
	{
		const std::scoped_lock lock(mutex);
		return state;
	}

	std::uint64_t NativeMCMRegistry::Revision() const
	{
		const std::scoped_lock lock(mutex);
		return state.revision;
	}

	Result<std::size_t> NativeMCMRegistry::Count(std::uint64_t a_session) const
	{
		const std::scoped_lock lock(mutex);
		if (!a_session || a_session != state.session)
			return std::unexpected(BridgeError{ BridgeErrorCode::kStaleSnapshot, "Native registry count session changed" });
		return state.count;
	}

	Result<bool> NativeMCMRegistry::Import(std::uint64_t a_session, std::vector<std::optional<NativeMCMEntry>> a_slots)
	{
		const std::scoped_lock lock(mutex);
		if (!a_session || a_session != state.session)
			return std::unexpected(BridgeError{ BridgeErrorCode::kStaleSnapshot, "Native registry import session changed" });
		if (state.revision != 1)
			return std::unexpected(BridgeError{ BridgeErrorCode::kBusy, "Native registry bootstrap already completed" });
		if (a_slots.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Native registry import exceeds compatibility index range" });
		decltype(instances)  importedInstances;
		decltype(identities) importedIdentities;
		for (std::size_t index = 0; index < a_slots.size(); ++index) {
			const auto& entry = a_slots[index];
			if (!entry)
				continue;
			if (!entry->instance || entry->identity.empty() ||
				!importedInstances.emplace(entry->instance, static_cast<std::int32_t>(index)).second ||
				!importedIdentities.emplace(entry->identity, entry->instance).second)
				return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Native registry import contains invalid or duplicate identities" });
		}
		state.count = importedInstances.size();
		state.slots = std::move(a_slots);
		instances = std::move(importedInstances);
		identities = std::move(importedIdentities);
		++state.revision;
		return true;
	}

	std::optional<NativeMCMEntry> NativeMCMRegistry::Find(std::uint64_t a_session, std::int32_t a_slot) const
	{
		const std::scoped_lock lock(mutex);
		if (!a_session || a_session != state.session || a_slot < 0 || static_cast<std::size_t>(a_slot) >= state.slots.size())
			return std::nullopt;
		return state.slots[static_cast<std::size_t>(a_slot)];
	}
}
