#include "MCMBridge/Core/NativeHostSession.h"

namespace MCMBridge
{
	bool NativeHostSession::SetOptionCursor(std::int32_t a_token, std::int32_t a_slot)
	{
		const std::scoped_lock lock(mutex);
		if (!Owns(a_token) || !hasPage)
			return false;
		// An invalid selection must not leave a previous control selected.
		optionCursor = a_slot >= 0 && static_cast<std::size_t>(a_slot) < working.buffers.optionFlags.size() ? a_slot : -1;
		return optionCursor >= 0;
	}

	std::optional<NativeHostCursorValue> NativeHostSession::ReadOptionCursor(std::int32_t a_token) const
	{
		const std::scoped_lock lock(mutex);
		if (!Owns(a_token) || !hasPage || optionCursor < 0)
			return std::nullopt;
		const auto index = static_cast<std::size_t>(optionCursor);
		if (index >= working.buffers.optionFlags.size() || index >= working.buffers.numericValues.size())
			return std::nullopt;
		return NativeHostCursorValue{ working.buffers.optionFlags[index] & 0xFF, working.buffers.numericValues[index] };
	}
}
