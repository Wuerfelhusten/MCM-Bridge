#pragma once

#include "MCMBridge/Core/Model.h"

#include <spdlog/spdlog.h>

namespace MCMBridge
{
	std::string      AuditValue(const MCMValue& a_value);
	std::string_view AuditIntent(WriteIntent a_intent);

	template <class... Args>
	void HostAudit(spdlog::format_string_t<Args...> a_format, Args&&... a_args) noexcept
	{
		try {
			spdlog::info(a_format, std::forward<Args>(a_args)...);
		} catch (...) {
		}
	}
}
