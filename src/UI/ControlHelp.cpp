#include "MCMBridge/UI/ControlHelp.h"

#include "MCMBridge/Core/SkyUIRichText.h"
#include "MCMBridge/Plugin/BridgeController.h"
#include "MCMBridge/UI/FrontendUI.h"

#include <format>

namespace
{
	std::string ValueText(const MCMBridge::MCMControl& a_control)
	{
		if (!a_control.displayValue.empty()) {
			return MCMBridge::PlainSkyUIText(a_control.displayValue);
		}
		return std::visit([](const auto& a_value) {
			using T = std::decay_t<decltype(a_value)>;
			if constexpr (std::is_same_v<T, std::monostate>) {
				return std::string{};
			} else if constexpr (std::is_same_v<T, bool>) {
				return std::string(a_value ? "On" : "Off");
			} else if constexpr (std::is_same_v<T, std::string>) {
				return MCMBridge::PlainSkyUIText(a_value);
			} else {
				return std::format("{}", a_value);
			}
		},
			a_control.value);
	}

	std::string FormatHelp(const MCMBridge::MCMControl& a_control)
	{
		auto                       result = MCMBridge::PlainSkyUIText(a_control.help);
		const auto                 value = ValueText(a_control);
		constexpr std::string_view token = "{value}";
		for (auto position = result.find(token); position != std::string::npos; position = result.find(token, position + value.size())) {
			result.replace(position, token.size(), value);
		}
		return result;
	}
}

namespace MCMBridge::ControlHelp
{
	void Render(const MCMControl& a_control)
	{
		if (!BridgeUI::IsItemHovered() || a_control.type == MCMControlType::kEmpty || a_control.type == MCMControlType::kHeader) {
			return;
		}
		if (!a_control.help.empty()) {
			const auto help = FormatHelp(a_control);
			BridgeUI::SetItemTooltip("%s", help.c_str());
		}
		if (a_control.identity.backend == MCMBackendKind::kClassicSkyUI) {
			BridgeController::GetSingleton().RequestControlHelp(a_control.identity, a_control.value);
		}
	}
}
