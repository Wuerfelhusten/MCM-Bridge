#include "MCMBridge/UI/InputSelector.h"

#include "SKSEMenuFramework.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <format>
#include <unordered_map>

namespace
{
	struct InputState
	{
		std::string            source;
		std::array<char, 512>  buffer{};
		MCMBridge::WriteStatus status{ MCMBridge::WriteStatus::kIdle };
	};

	std::unordered_map<std::string, InputState> states;

	void CopyValue(InputState& a_state, std::string_view a_value)
	{
		a_state.buffer.fill('\0');
		const auto count = (std::min)(a_value.size(), a_state.buffer.size() - 1);
		std::memcpy(a_state.buffer.data(), a_value.data(), count);
	}
}

namespace MCMBridge::InputSelector
{
	std::optional<std::string> Render(const MCMControl& a_control, bool a_enabled)
	{
		const auto*       value = std::get_if<std::string>(&a_control.value);
		const std::string source = value ? *value : std::string{};
		auto [entry, inserted] = states.try_emplace(a_control.identity.stableID);
		const auto failed = a_control.writeStatus == WriteStatus::kRejected ||
		                    a_control.writeStatus == WriteStatus::kTimedOut || a_control.writeStatus == WriteStatus::kStaleSnapshot;
		if (inserted || entry->second.source != source || (failed && entry->second.status != a_control.writeStatus)) {
			entry->second.source = source;
			entry->second.status = a_control.writeStatus;
			const auto initial = inserted && a_control.input && a_control.input->availability == MetadataAvailability::kAvailable ?
			                         std::string_view(a_control.input->startText) :
			                         std::string_view(source);
			CopyValue(entry->second, initial);
		}

		const auto widgetID = std::format("##{}", a_control.identity.stableID);
		ImGuiMCP::BeginDisabled(!a_enabled);
		const auto submitted = ImGuiMCP::InputText(
			widgetID.c_str(), entry->second.buffer.data(), entry->second.buffer.size(), ImGuiMCP::ImGuiInputTextFlags_EnterReturnsTrue);
		const auto committed = submitted || ImGuiMCP::IsItemDeactivatedAfterEdit();
		ImGuiMCP::EndDisabled();
		entry->second.status = a_control.writeStatus;
		if (a_enabled && committed) {
			return std::string(entry->second.buffer.data());
		}
		return std::nullopt;
	}
}
