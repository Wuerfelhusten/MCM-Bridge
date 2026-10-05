#pragma once

#include "MCMBridge/Core/Model.h"

#include <deque>
#include <functional>
#include <mutex>
#include <optional>

namespace MCMBridge
{
	class WriteQueue
	{
	public:
		void                        Push(WriteCommand a_command);
		void                        PushFront(WriteCommand a_command);
		std::optional<WriteCommand> TryPop();
		std::optional<WriteCommand> Remove(std::uint64_t a_pauseTicket);
		std::vector<WriteCommand>   RemoveIf(const std::function<bool(const WriteCommand&)>& a_predicate);
		void                        Clear();

	private:
		std::mutex               mutex;
		std::deque<WriteCommand> commands;
	};
}
