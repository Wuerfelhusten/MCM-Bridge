#include "MCMBridge/Core/HostedPageMerge.h"

#include <algorithm>

namespace
{
	bool Compatible(MCMBridge::MCMControlType a_previous, MCMBridge::MCMControlType a_current)
	{
		return a_previous == a_current ||
		       (a_previous == MCMBridge::MCMControlType::kStepper && a_current == MCMBridge::MCMControlType::kText);
	}

	const MCMBridge::MCMControl* FindPrevious(
		const MCMBridge::MCMPage&    a_page,
		const MCMBridge::MCMControl& a_current)
	{
		const auto stable = std::ranges::find(a_page.controls, a_current.identity.stableID, [](const auto& a_control) {
			return a_control.identity.stableID;
		});
		if (stable != a_page.controls.end()) {
			return std::addressof(*stable);
		}
		const auto positional = std::ranges::find_if(a_page.controls, [&](const auto& a_control) {
			return a_control.identity.optionIndex == a_current.identity.optionIndex &&
			       a_control.identity.stateName == a_current.identity.stateName && Compatible(a_control.type, a_current.type);
		});
		return positional != a_page.controls.end() ? std::addressof(*positional) : nullptr;
	}

	std::int32_t FindSelected(
		const MCMBridge::MCMControl& a_current,
		const MCMBridge::MCMControl& a_previous)
	{
		if (!a_previous.menu) {
			return -1;
		}
		const auto find = [&](const auto& a_options, std::string_view a_value) {
			const auto found = std::ranges::find(a_options, a_value);
			return found == a_options.end() ? -1 : static_cast<std::int32_t>(std::distance(a_options.begin(), found));
		};
		if (const auto raw = find(a_previous.menu->options, a_current.displayValue); raw >= 0) {
			return raw;
		}
		if (const auto shortName = find(a_previous.menu->shortNames, a_current.displayValue); shortName >= 0) {
			return shortName;
		}
		if (const auto display = find(a_previous.menu->displayOptions, a_current.displayValue); display >= 0) {
			return display;
		}
		if (const auto* text = std::get_if<std::string>(&a_current.value)) {
			if (const auto raw = find(a_previous.menu->options, *text); raw >= 0) {
				return raw;
			}
		}
		if (const auto* index = std::get_if<std::int32_t>(&a_previous.value)) {
			return *index;
		}
		return a_previous.menu->selectedIndex;
	}

	void CopySharedMetadata(const MCMBridge::MCMControl& a_previous, MCMBridge::MCMControl& a_current)
	{
		a_current.identity.stableID = a_previous.identity.stableID;
		a_current.identity.backend = a_previous.identity.backend;
		a_current.identity.explicitID = a_previous.identity.explicitID;
		a_current.identity.confidence = a_previous.identity.confidence;
		a_current.help = a_previous.help;
		a_current.defaultValue = a_previous.defaultValue;
		a_current.source = a_previous.source;
		a_current.action = a_previous.action;
		a_current.condition = a_previous.condition;
		a_current.groupControl = a_previous.groupControl;
		a_current.ignoreConflicts = a_previous.ignoreConflicts;
		a_current.writeStatus = a_previous.writeStatus;
	}

	void MergeTypedMetadata(const MCMBridge::MCMControl& a_previous, MCMBridge::MCMControl& a_current)
	{
		if (a_previous.type == MCMBridge::MCMControlType::kStepper && a_current.type == MCMBridge::MCMControlType::kText) {
			a_current.type = MCMBridge::MCMControlType::kStepper;
			a_current.displayValue = std::get_if<std::string>(&a_current.value) ? std::get<std::string>(a_current.value) : a_current.displayValue;
		}
		if (a_current.type == MCMBridge::MCMControlType::kSlider && a_previous.slider) {
			const auto format = a_current.slider ? a_current.slider->format : std::string{};
			a_current.slider = a_previous.slider;
			if (const auto* value = std::get_if<float>(&a_current.value)) {
				a_current.slider->start = *value;
			}
			if (!format.empty()) {
				a_current.slider->format = format;
			}
		}
		if ((a_current.type == MCMBridge::MCMControlType::kMenu || a_current.type == MCMBridge::MCMControlType::kStepper) && a_previous.menu) {
			const auto selected = FindSelected(a_current, a_previous);
			a_current.menu = a_previous.menu;
			a_current.menu->selectedIndex = selected;
			a_current.value = selected;
		}
		if (a_current.type == MCMBridge::MCMControlType::kColor && a_previous.color) {
			a_current.color = a_previous.color;
			if (const auto* value = std::get_if<std::uint32_t>(&a_current.value)) {
				a_current.color->start = *value;
			}
		}
		if (a_current.type == MCMBridge::MCMControlType::kInput && a_previous.input) {
			a_current.input = a_previous.input;
		}
	}

	MCMBridge::WriteCapability MergeWriteCapability(
		const MCMBridge::MCMControl& a_previous, const MCMBridge::MCMControl& a_current)
	{
		using MCMBridge::MCMBackendKind;
		using MCMBridge::MCMControlType;
		using MCMBridge::MetadataAvailability;
		using MCMBridge::WriteCapability;
		if (a_previous.writeCapability == WriteCapability::kUnsupported)
			return WriteCapability::kUnsupported;
		if (a_current.disabled)
			return WriteCapability::kDisabled;
		if (a_previous.writeCapability != WriteCapability::kDisabled)
			return a_previous.writeCapability;

		// Disabled is transient; recover editability only after restoring the control's metadata.
		switch (a_current.type) {
		case MCMControlType::kToggle:
		case MCMControlType::kKeymap:
		case MCMControlType::kInput:
			return WriteCapability::kWritable;
		case MCMControlType::kText:
			return a_current.identity.backend == MCMBackendKind::kClassicSkyUI || a_current.action ?
			           WriteCapability::kWritable :
			           WriteCapability::kReadOnly;
		case MCMControlType::kSlider:
			return a_current.slider && a_current.slider->availability == MetadataAvailability::kAvailable ?
			           WriteCapability::kWritable :
			           WriteCapability::kReadOnly;
		case MCMControlType::kMenu:
		case MCMControlType::kStepper:
			return a_current.menu && !a_current.menu->options.empty() ?
			           WriteCapability::kWritable :
			           WriteCapability::kMissingOptions;
		case MCMControlType::kColor:
			return a_current.color && a_current.color->availability == MetadataAvailability::kAvailable ?
			           WriteCapability::kWritable :
			           WriteCapability::kReadOnly;
		default:
			return WriteCapability::kReadOnly;
		}
	}
}

namespace MCMBridge
{
	void MergeHostedPageState(const MCMPage& a_previous, MCMPage& a_current)
	{
		if (a_current.title.empty()) {
			a_current.title = a_previous.title;
		}
		for (auto& control : a_current.controls) {
			const auto* old = FindPrevious(a_previous, control);
			if (!old) {
				continue;
			}
			CopySharedMetadata(*old, control);
			MergeTypedMetadata(*old, control);
			control.writeCapability = MergeWriteCapability(*old, control);
		}
	}
}
