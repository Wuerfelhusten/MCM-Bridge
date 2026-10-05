#pragma once

#include "MCMBridge/Core/Model.h"

#include <atomic>
#include <memory>
#include <mutex>

namespace MCMBridge
{
	class SnapshotStore
	{
	public:
		SnapshotStore();

		std::shared_ptr<const MCMSnapshot> Get() const;
		std::shared_ptr<const MCMSnapshot> Publish(MCMSnapshot a_snapshot);
		std::shared_ptr<const MCMSnapshot> Reset(std::string a_reason = {});
		std::shared_ptr<const MCMSnapshot> SetRefreshing(bool a_refreshing);
		std::shared_ptr<const MCMSnapshot> ReplacePage(std::string_view a_modID, MCMPage a_page);
		std::shared_ptr<const MCMSnapshot> SetControlHelp(std::string_view a_settingID, std::string a_help);
		std::shared_ptr<const MCMSnapshot> SetWriteStatus(
			std::string_view           a_settingID,
			WriteStatus                a_status,
			std::optional<MCMValue>    a_confirmedValue = std::nullopt,
			std::optional<std::string> a_confirmedDisplayValue = std::nullopt);

	private:
		std::shared_ptr<const MCMSnapshot> PublishLocked(MCMSnapshot a_snapshot, bool a_advanceGeneration = true);

		mutable std::mutex                              writeMutex;
		std::atomic<std::shared_ptr<const MCMSnapshot>> current;
	};
}
