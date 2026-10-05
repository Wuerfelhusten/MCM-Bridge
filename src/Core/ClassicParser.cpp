#include "MCMBridge/Core/ClassicParser.h"

#include "MCMBridge/Core/StableId.h"

#include <algorithm>

namespace
{
	constexpr std::int32_t disabledFlag = 1;
	constexpr std::int32_t hiddenFlag = 2;
	constexpr std::int32_t withUnmapFlag = 4;

	MCMBridge::MCMControlType DecodeType(std::int32_t a_skyUIType)
	{
		using MCMBridge::MCMControlType;
		switch (a_skyUIType) {
		case 0:
			return MCMControlType::kEmpty;
		case 1:
			return MCMControlType::kHeader;
		case 2:
			return MCMControlType::kText;
		case 3:
			return MCMControlType::kToggle;
		case 4:
			return MCMControlType::kSlider;
		case 5:
			return MCMControlType::kMenu;
		case 6:
			return MCMControlType::kColor;
		case 7:
			return MCMControlType::kKeymap;
		case 8:
			return MCMControlType::kInput;
		default:
			return MCMControlType::kUnknown;
		}
	}

	std::size_t FindOptionCount(const std::vector<std::int32_t>& a_flags)
	{
		std::size_t count = 0;
		for (std::size_t index = 0; index < a_flags.size(); ++index) {
			if (a_flags[index] != 0) {
				count = index + 1;
			}
		}
		return count;
	}
}

namespace MCMBridge
{
	Result<MCMPage> ParseClassicPage(const ClassicPageContext& a_context, const ClassicPageBuffers& a_buffers)
	{
		const auto size = std::min({ a_buffers.optionFlags.size(), a_buffers.labels.size(),
			a_buffers.stringValues.size(), a_buffers.numericValues.size() });
		if (size == 0) {
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Classic page buffers are empty" });
		}

		MCMPage page;
		page.stableID = MakeClassicPageID(a_context.modID, a_context.pageName, a_context.pageIndex);
		page.rawName = a_context.pageName;
		page.displayName = a_context.pageName;
		page.index = a_context.pageIndex;

		const auto optionCount = std::min(size, FindOptionCount(a_buffers.optionFlags));
		page.controls.reserve(optionCount);
		for (std::size_t index = 0; index < optionCount; ++index) {
			const auto packedFlags = a_buffers.optionFlags[index];
			const auto type = DecodeType(packedFlags & 0xFF);
			if (type == MCMControlType::kUnknown) {
				continue;
			}

			const auto flags = packedFlags >> 8;
			MCMControl control;
			control.type = type;
			control.label = a_buffers.labels[index];
			control.disabled = (flags & disabledFlag) != 0;
			control.hidden = (flags & hiddenFlag) != 0;
			control.allowUnmap = (flags & withUnmapFlag) != 0;
			control.layout.position = static_cast<std::int32_t>(index);
			control.layout.column = static_cast<std::int32_t>(index % 2);

			const auto stateName = index < a_buffers.stateNames.size() ? a_buffers.stateNames[index] : std::string{};
			control.identity = SettingIdentity{
				.stableID = MakeClassicControlID(page.stableID, stateName, static_cast<std::uint16_t>(index), type, control.label),
				.backend = MCMBackendKind::kClassicSkyUI,
				.ownerPlugin = a_context.ownerPlugin,
				.questFormID = a_context.questFormID,
				.scriptName = a_context.scriptName,
				.stateName = stateName,
				.pageKey = a_context.pageName,
				.pageIndex = a_context.pageIndex,
				.optionIndex = static_cast<std::uint16_t>(index),
				.confidence = stateName.empty() ? IdentityConfidence::kLow : IdentityConfidence::kHigh
			};

			switch (type) {
			case MCMControlType::kText:
				control.value = a_buffers.stringValues[index];
				control.writeCapability = control.disabled ? WriteCapability::kDisabled : WriteCapability::kWritable;
				break;
			case MCMControlType::kToggle:
				control.value = a_buffers.numericValues[index] != 0.0F;
				control.writeCapability = control.disabled ? WriteCapability::kDisabled : WriteCapability::kWritable;
				break;
			case MCMControlType::kSlider:
				control.value = a_buffers.numericValues[index];
				control.slider = SliderMetadata{
					.start = a_buffers.numericValues[index],
					.format = a_buffers.stringValues[index],
					.availability = MetadataAvailability::kMissing
				};
				control.writeCapability = control.disabled ? WriteCapability::kDisabled : WriteCapability::kWritable;
				break;
			case MCMControlType::kMenu:
				control.displayValue = a_buffers.stringValues[index];
				control.menu = MenuMetadata{};
				control.writeCapability = control.disabled ? WriteCapability::kDisabled : WriteCapability::kMissingOptions;
				break;
			case MCMControlType::kColor:
				control.value = static_cast<std::uint32_t>(a_buffers.numericValues[index]);
				control.color = ColorMetadata{};
				control.writeCapability = control.disabled ? WriteCapability::kDisabled : WriteCapability::kWritable;
				break;
			case MCMControlType::kKeymap:
				control.value = static_cast<std::int32_t>(a_buffers.numericValues[index]);
				control.writeCapability = control.disabled ? WriteCapability::kDisabled : WriteCapability::kWritable;
				break;
			case MCMControlType::kInput:
				control.value = a_buffers.stringValues[index];
				control.input = InputMetadata{};
				control.writeCapability = control.disabled ? WriteCapability::kDisabled : WriteCapability::kWritable;
				break;
			default:
				break;
			}

			page.controls.push_back(std::move(control));
		}

		return page;
	}
}
