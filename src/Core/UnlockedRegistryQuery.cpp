#include "MCMBridge/Core/UnlockedRegistryQuery.h"
#include <unordered_set>

namespace MCMBridge
{
	UnlockedRegistryQuery::UnlockedRegistryQuery(CountReader a_count, EntryReader a_entry, std::function<Clock::time_point()> a_now) :
		count(std::move(a_count)), entry(std::move(a_entry)), now(std::move(a_now)) {}

	void UnlockedRegistryQuery::Start()
	{
		if (started)
			return;
		started = true;
		deadline = now() + std::chrono::seconds(30);
		BeginPass();
	}

	bool UnlockedRegistryQuery::Active()
	{
		if (!result && started && now() >= deadline) {
			Fail({ BridgeErrorCode::kTimedOut, "MCM Unlocked registry query timed out" });
		}
		return !result.has_value();
	}

	void UnlockedRegistryQuery::Fail(BridgeError a_error)
	{
		if (!result) {
			result = std::unexpected(std::move(a_error));
			if (completion)
				completion();
		}
	}

	void UnlockedRegistryQuery::Cancel()
	{
		Fail({ BridgeErrorCode::kStaleSnapshot, "MCM Unlocked registry query invalidated" });
	}

	Result<UnlockedRegistryQuery::Entries> UnlockedRegistryQuery::Poll()
	{
		if (Active())
			return std::unexpected(BridgeError{ BridgeErrorCode::kBusy, "Reading MCM Unlocked registry" });
		return *result;
	}

	void UnlockedRegistryQuery::BeginPass()
	{
		count([weak = weak_from_this()](Result<std::int32_t> a_count) {
			auto self = weak.lock();
			if (!self || !self->Active())
				return;
			if (!a_count)
				return self->Fail(a_count.error());
			if (*a_count < 0 || *a_count > 65536) {
				return self->Fail({ BridgeErrorCode::kInvalidData, "Invalid MCM Unlocked registry size" });
			}
			if (self->verification && static_cast<std::size_t>(*a_count) != self->first.size()) {
				return self->Fail({ BridgeErrorCode::kStaleSnapshot, "MCM Unlocked registry changed while reading" });
			}
			self->current.assign(static_cast<std::size_t>(*a_count), {});
			self->next = 0;
			self->pending = 0;
			self->Pump();
		});
	}

	void UnlockedRegistryQuery::Pump()
	{
		if (!Active() || pumping)
			return;
		pumping = true;
		while (pending < 8 && next < current.size()) {
			const auto index = next++;
			++pending;
			entry(static_cast<std::int32_t>(index), [weak = weak_from_this(), index](Result<UnlockedRegistryEntry> a_entry) {
				auto self = weak.lock();
				if (!self || !self->Active())
					return;
				if (!a_entry)
					return self->Fail(a_entry.error());
				if (a_entry->id.empty() || a_entry->marker == 0) {
					return self->Fail({ BridgeErrorCode::kStaleSnapshot, "MCM Unlocked registry entry is unavailable" });
				}
				self->current[index] = std::move(*a_entry);
				--self->pending;
				self->Pump();
			});
			if (!Active()) {
				pumping = false;
				return;
			}
		}
		pumping = false;
		if (pending == 0 && next == current.size())
			CompletePass();
	}

	void UnlockedRegistryQuery::CompletePass()
	{
		std::unordered_set<std::string>   ids;
		std::unordered_set<std::uint64_t> markers;
		for (const auto& record : current) {
			if (!ids.insert(record.id).second || !markers.insert(record.marker).second) {
				return Fail({ BridgeErrorCode::kInvalidData, "Duplicate MCM Unlocked registry identity" });
			}
		}
		if (!verification) {
			first = std::move(current);
			verification = true;
			BeginPass();
			return;
		}
		for (std::size_t i = 0; i < first.size(); ++i) {
			if (first[i].id != current[i].id || first[i].marker != current[i].marker) {
				return Fail({ BridgeErrorCode::kStaleSnapshot, "MCM Unlocked registry changed while reading" });
			}
		}
		count([weak = weak_from_this()](Result<std::int32_t> a_count) {
			auto self = weak.lock();
			if (!self || !self->Active())
				return;
			if (!a_count)
				return self->Fail(a_count.error());
			if (*a_count < 0 || static_cast<std::size_t>(*a_count) != self->current.size()) {
				return self->Fail({ BridgeErrorCode::kStaleSnapshot, "MCM Unlocked registry changed before publication" });
			}
			self->result = std::move(self->current);
			if (self->completion)
				self->completion();
		});
	}
}
