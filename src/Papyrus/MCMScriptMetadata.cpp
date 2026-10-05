#include "MCMBridge/Papyrus/MCMScript.h"

namespace MCMBridge
{
	Result<SliderMetadata> MCMScript::ReadSliderMetadata(std::uint16_t a_optionIndex) const
	{
		if (!IsMenuReady(a_optionIndex)) {
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Slider dialog data is not ready" });
		}
		std::array<float, 5> values{};
		for (std::size_t index = 0; index < values.size(); ++index) {
			auto value = ReadNumber("_sliderParams", index);
			if (!value) {
				return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Slider parameters are incomplete" });
			}
			values[index] = *value;
		}
		return SliderMetadata{
			.start = values[0],
			.defaultValue = values[1],
			.minimum = values[2],
			.maximum = values[3],
			.step = values[4],
			.availability = MetadataAvailability::kAvailable
		};
	}

	Result<MenuMetadata> MCMScript::ReadMenuMetadata(std::uint16_t a_optionIndex) const
	{
		if (!IsMenuReady(a_optionIndex)) {
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Menu dialog data is not ready" });
		}
		auto selected = ReadNumber("_menuParams", 0);
		auto defaultIndex = ReadNumber("_menuParams", 1);
		if (!selected || !defaultIndex) {
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Menu parameters are incomplete" });
		}
		return MenuMetadata{
			.selectedIndex = static_cast<std::int32_t>(*selected),
			.defaultIndex = static_cast<std::int32_t>(*defaultIndex),
			.availability = MetadataAvailability::kMissing
		};
	}

	Result<ColorMetadata> MCMScript::ReadColorMetadata(std::uint16_t a_optionIndex) const
	{
		if (!IsMenuReady(a_optionIndex)) {
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Color dialog data is not ready" });
		}
		auto values = ReadArray("_colorParams");
		if (!values || values->size() < 2 || !(*values)[0].IsInt() || !(*values)[1].IsInt()) {
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Color parameters are incomplete" });
		}
		return ColorMetadata{
			.start = static_cast<std::uint32_t>((*values)[0].GetSInt()),
			.defaultValue = static_cast<std::uint32_t>((*values)[1].GetSInt()),
			.availability = MetadataAvailability::kAvailable
		};
	}

	Result<InputMetadata> MCMScript::ReadInputMetadata(std::uint16_t a_optionIndex) const
	{
		if (!IsMenuReady(a_optionIndex)) {
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Input dialog data is not ready" });
		}
		const auto startText = ReadScalarString("_inputStartText");
		if (!startText) {
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Input start text is unavailable" });
		}
		return InputMetadata{ .startText = *startText, .availability = MetadataAvailability::kAvailable };
	}

}
