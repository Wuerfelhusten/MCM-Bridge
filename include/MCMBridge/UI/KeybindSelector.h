#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

namespace MCMBridge::KeybindSelector
{
	enum class Action
	{
		kSetValue,
		kReset
	};

	struct Edit
	{
		Action       action{ Action::kSetValue };
		std::int32_t value{};
	};

	bool                Install();
	std::optional<Edit> Render(
		std::string_view a_stableID,
		std::string_view a_label,
		std::int32_t     a_currentValue,
		bool             a_enabled,
		bool             a_allowUnmap);
}
