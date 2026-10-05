#include "MCMBridge/Core/HostAudit.h"

#include <nlohmann/json.hpp>

namespace MCMBridge
{
	std::string AuditValue(const MCMValue& a_value)
	{
		return std::visit([](const auto& a_entry) -> std::string {
			using Value = std::decay_t<decltype(a_entry)>;
			if constexpr (std::is_same_v<Value, std::monostate>)
				return "none";
			else {
				constexpr auto type = std::is_same_v<Value, bool>          ? "bool" :
				                      std::is_same_v<Value, float>         ? "float" :
				                      std::is_same_v<Value, std::int32_t>  ? "int" :
				                      std::is_same_v<Value, std::uint32_t> ? "uint" :
				                                                             "string";
				return std::string(type) + ":" + nlohmann::json(a_entry).dump(-1, ' ', true, nlohmann::json::error_handler_t::replace);
			}
		},
			a_value);
	}

	std::string_view AuditIntent(WriteIntent a_intent)
	{
		switch (a_intent) {
		case WriteIntent::kSetValue:
			return "SET_VALUE";
		case WriteIntent::kActivate:
			return "ACTIVATE";
		case WriteIntent::kReset:
			return "RESET";
		}
		return "UNKNOWN";
	}
}
