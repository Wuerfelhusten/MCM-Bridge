#include "MCMBridge/Core/NativeHostSession.h"

namespace MCMBridge
{
	bool NativeHostSession::UpdateControl(std::int32_t a_token, std::int32_t a_index,
		std::optional<std::string> a_text, std::optional<float> a_value, std::optional<std::int32_t> a_flags,
		std::optional<std::int32_t> a_expectedType)
	{
		const std::scoped_lock lock(mutex);
		if (!Owns(a_token) || !hasPage || building || a_index < 0 || a_index >= 128 ||
			(a_flags && (*a_flags < 0 || *a_flags > 0x7FFFFF)))
			return false;
		const auto index = static_cast<std::size_t>(a_index);
		auto&      buffers = working.buffers;
		const auto type = buffers.optionFlags[index] & 0xFF;
		if (type < 1 || type > 8 || (a_expectedType && type != *a_expectedType))
			return false;
		if (a_text)
			buffers.stringValues[index] = std::move(*a_text);
		if (a_value)
			buffers.numericValues[index] = *a_value;
		if (a_flags)
			buffers.optionFlags[index] = type + *a_flags * 256;
		return true;
	}
}
