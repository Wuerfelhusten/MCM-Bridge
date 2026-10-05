#include "MCMBridge/Snapshot/SnapshotStore.h"

#include <algorithm>

namespace MCMBridge
{
	SnapshotStore::SnapshotStore()
	{
		auto initial = std::make_shared<MCMSnapshot>();
		initial->createdAt = std::chrono::steady_clock::now();
		current.store(std::move(initial));
	}

	std::shared_ptr<const MCMSnapshot> SnapshotStore::Get() const
	{
		return current.load();
	}

	std::shared_ptr<const MCMSnapshot> SnapshotStore::Publish(MCMSnapshot a_snapshot)
	{
		const std::scoped_lock lock(writeMutex);
		return PublishLocked(std::move(a_snapshot));
	}

	std::shared_ptr<const MCMSnapshot> SnapshotStore::Reset(std::string a_reason)
	{
		MCMSnapshot snapshot;
		if (!a_reason.empty()) {
			snapshot.diagnostics.push_back({ DiagnosticSeverity::kInfo, "lifecycle", std::move(a_reason) });
		}
		return Publish(std::move(snapshot));
	}

	std::shared_ptr<const MCMSnapshot> SnapshotStore::SetRefreshing(bool a_refreshing)
	{
		const std::scoped_lock lock(writeMutex);
		auto                   snapshot = *current.load();
		snapshot.refreshing = a_refreshing;
		return PublishLocked(std::move(snapshot));
	}

	std::shared_ptr<const MCMSnapshot> SnapshotStore::ReplacePage(std::string_view a_modID, MCMPage a_page)
	{
		const std::scoped_lock lock(writeMutex);
		const auto             previous = current.load();
		for (const auto& mod : previous->mods) {
			if (mod.stableID != a_modID) {
				continue;
			}
			const auto found = std::ranges::find_if(mod.pages, [&](const auto& a_current) {
				return a_current.stableID == a_page.stableID || a_current.index == a_page.index;
			});
			if (found != mod.pages.end()) {
				// Successful live reads do not invalidate queued commands when nothing changed.
				if (*found == a_page)
					return previous;
				auto       snapshot = *previous;
				const auto modIndex = static_cast<std::size_t>(&mod - previous->mods.data());
				const auto pageIndex = static_cast<std::size_t>(found - mod.pages.begin());
				snapshot.mods[modIndex].pages[pageIndex] = std::move(a_page);
				return PublishLocked(std::move(snapshot));
			}
			return current.load();
		}
		return current.load();
	}

	std::shared_ptr<const MCMSnapshot> SnapshotStore::SetWriteStatus(
		std::string_view           a_settingID,
		WriteStatus                a_status,
		std::optional<MCMValue>    a_confirmedValue,
		std::optional<std::string> a_confirmedDisplayValue)
	{
		const std::scoped_lock lock(writeMutex);
		auto                   snapshot = *current.load();
		for (auto& mod : snapshot.mods) {
			for (auto& page : mod.pages) {
				for (auto& control : page.controls) {
					if (control.identity.stableID == a_settingID) {
						control.writeStatus = a_status;
						if (a_confirmedValue) {
							control.value = std::move(*a_confirmedValue);
						}
						if (a_confirmedDisplayValue) {
							control.displayValue = std::move(*a_confirmedDisplayValue);
						}
						return PublishLocked(std::move(snapshot), a_confirmedValue.has_value());
					}
				}
			}
		}
		return current.load();
	}

	std::shared_ptr<const MCMSnapshot> SnapshotStore::SetControlHelp(std::string_view a_settingID, std::string a_help)
	{
		const std::scoped_lock lock(writeMutex);
		auto                   snapshot = *current.load();
		for (auto& mod : snapshot.mods) {
			for (auto& page : mod.pages) {
				const auto found = std::ranges::find(page.controls, a_settingID, [](const auto& a_control) {
					return a_control.identity.stableID;
				});
				if (found != page.controls.end()) {
					found->help = std::move(a_help);
					return PublishLocked(std::move(snapshot), false);
				}
			}
		}
		return current.load();
	}

	std::shared_ptr<const MCMSnapshot> SnapshotStore::PublishLocked(MCMSnapshot a_snapshot, bool a_advanceGeneration)
	{
		const auto previous = current.load();
		a_snapshot.generation = previous ? previous->generation + (a_advanceGeneration ? 1U : 0U) : 1;
		a_snapshot.createdAt = std::chrono::steady_clock::now();
		auto next = std::make_shared<const MCMSnapshot>(std::move(a_snapshot));
		current.store(next);
		return next;
	}
}
