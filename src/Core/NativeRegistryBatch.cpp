#include "MCMBridge/Core/NativeMCMRegistry.h"

#include <limits>

namespace MCMBridge
{
	Result<std::vector<std::int32_t>> NativeMCMRegistry::RegisterBatch(std::uint64_t a_session, std::vector<NativeMCMEntry> a_entries)
	{
		const std::scoped_lock lock(mutex);
		if (!a_session || a_session != state.session)
			return std::unexpected(BridgeError{ BridgeErrorCode::kStaleSnapshot, "Native registry batch session changed" });
		std::vector<std::int32_t> slots;
		if (a_entries.empty())
			return slots;
		// Build privately so a late conflict or allocation failure cannot expose a prefix.
		auto candidate = state;
		auto candidateInstances = instances;
		auto candidateIdentities = identities;
		slots.reserve(a_entries.size());
		for (auto& entry : a_entries) {
			if (!entry.instance || entry.identity.empty())
				return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Native batch registration has no identity" });
			if (const auto found = candidateInstances.find(entry.instance); found != candidateInstances.end()) {
				auto& current = *candidate.slots[static_cast<std::size_t>(found->second)];
				if (current.identity != entry.identity)
					return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Native batch instance identity changed" });
				if (current.name != entry.name) {
					current.name = std::move(entry.name);
					++candidate.revision;
				}
				slots.push_back(found->second);
				continue;
			}
			if (candidateIdentities.contains(entry.identity))
				return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Native batch identity belongs to another instance" });
			if (candidate.slots.size() >= static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
				return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "Native batch compatibility indices exhausted" });
			const auto slot = static_cast<std::int32_t>(candidate.slots.size());
			candidateIdentities.emplace(entry.identity, entry.instance);
			candidateInstances.emplace(entry.instance, slot);
			candidate.slots.emplace_back(std::move(entry));
			slots.push_back(slot);
			++candidate.count;
			++candidate.revision;
		}
		state = std::move(candidate);
		instances = std::move(candidateInstances);
		identities = std::move(candidateIdentities);
		return slots;
	}
}
