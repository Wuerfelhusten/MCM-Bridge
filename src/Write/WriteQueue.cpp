#include "MCMBridge/Write/WriteQueue.h"

#include "MCMBridge/Write/WriteTiming.h"

#include <algorithm>

namespace MCMBridge
{
	void WriteQueue::Push(WriteCommand a_command)
	{
		const std::scoped_lock lock(mutex);
		commands.push_back(std::move(a_command));
	}

	void WriteQueue::PushFront(WriteCommand a_command)
	{
		const std::scoped_lock lock(mutex);
		commands.push_front(std::move(a_command));
	}

	std::optional<WriteCommand> WriteQueue::TryPop()
	{
		const std::scoped_lock lock(mutex);
		if (commands.empty()) {
			return std::nullopt;
		}
		auto command = std::move(commands.front());
		commands.pop_front();
		return command;
	}

	std::optional<WriteCommand> WriteQueue::Remove(std::uint64_t a_pauseTicket)
	{
		const std::scoped_lock lock(mutex);
		const auto             found = std::ranges::find(commands, a_pauseTicket, &WriteCommand::pauseTicket);
		if (found == commands.end())
			return std::nullopt;
		auto command = std::move(*found);
		commands.erase(found);
		return command;
	}

	std::vector<WriteCommand> WriteQueue::RemoveIf(const std::function<bool(const WriteCommand&)>& a_predicate)
	{
		const std::scoped_lock    lock(mutex);
		std::vector<WriteCommand> removed;
		for (auto it = commands.begin(); it != commands.end();) {
			if (a_predicate(*it)) {
				removed.push_back(std::move(*it));
				it = commands.erase(it);
			} else {
				++it;
			}
		}
		return removed;
	}

	void WriteQueue::Clear()
	{
		std::deque<WriteCommand> cancelled;
		{
			const std::scoped_lock lock(mutex);
			cancelled.swap(commands);
		}
		for (const auto& command : cancelled) {
			if (command.timing) {
				command.timing->Finish(WriteTimingOutcome::kCancelled, "Write queue cleared");
			}
		}
	}
}
