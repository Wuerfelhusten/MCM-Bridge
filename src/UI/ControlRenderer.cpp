#include "MCMBridge/UI/ControlRenderer.h"

#include "MCMBridge/Core/Color.h"
#include "MCMBridge/Core/SkyUIFormat.h"
#include "MCMBridge/Core/SkyUIRichText.h"
#include "MCMBridge/UI/ControlHelp.h"
#include "MCMBridge/UI/ControlRowRenderer.h"
#include "MCMBridge/UI/ControlWrite.h"
#include "MCMBridge/UI/FrontendUI.h"
#include "MCMBridge/UI/IconButton.h"
#include "MCMBridge/UI/InputSelector.h"
#include "MCMBridge/UI/KeybindSelector.h"
#include "MCMBridge/UI/RichTextRenderer.h"

#include <unordered_map>

namespace
{
	struct Draft
	{
		MCMBridge::MCMValue    source;
		MCMBridge::MCMValue    value;
		MCMBridge::WriteStatus sourceStatus{ MCMBridge::WriteStatus::kIdle };
	};

	std::unordered_map<std::string, Draft> drafts;
	thread_local float                     rowEnd{};

	MCMBridge::MCMValue& GetDraft(const MCMBridge::MCMControl& a_control)
	{
		auto [entry, inserted] = drafts.try_emplace(
			a_control.identity.stableID,
			Draft{ a_control.value, a_control.value, a_control.writeStatus });
		const auto failed = a_control.writeStatus == MCMBridge::WriteStatus::kRejected ||
		                    a_control.writeStatus == MCMBridge::WriteStatus::kTimedOut ||
		                    a_control.writeStatus == MCMBridge::WriteStatus::kStaleSnapshot;
		if (!inserted && (entry->second.source != a_control.value ||
							 (failed && entry->second.sourceStatus != a_control.writeStatus))) {
			entry->second = { a_control.value, a_control.value, a_control.writeStatus };
		} else {
			entry->second.sourceStatus = a_control.writeStatus;
		}
		return entry->second.value;
	}

	void SetDraft(const MCMBridge::MCMControl& a_control, MCMBridge::MCMValue a_value)
	{
		auto& draft = drafts[a_control.identity.stableID];
		draft.source = a_control.value;
		draft.value = std::move(a_value);
		draft.sourceStatus = a_control.writeStatus;
	}

	void AlignRight(float a_width)
	{
		const auto cursor = BridgeUI::GetCursorPosX();
		const auto available = BridgeUI::GetContentRegionAvail().x;
		BridgeUI::SetCursorPosX(cursor + (std::max)(0.0F, available - a_width));
	}

	void RenderValueRow(std::string_view a_label, std::string_view a_value)
	{
		if (MCMBridge::renderFrontend == MCMBridge::Frontend::kFlick) {
			const auto value = MCMBridge::ParseSkyUIRichText(a_value);
			MCMBridge::ControlRowRenderer::BeginRow(a_label, BridgeUI::CalcTextSize(value.plainText.c_str()).x, 0);
			MCMBridge::RichTextRenderer::RenderDisabled(value);
			return;
		}
		const auto plainLabel = MCMBridge::PlainSkyUIText(a_label);
		if (!plainLabel.empty()) {
			MCMBridge::RichTextRenderer::Render(a_label);
		}
		if (a_value.empty()) {
			return;
		}
		if (!plainLabel.empty()) {
			BridgeUI::SameLine();
		}
		const auto richValue = MCMBridge::ParseSkyUIRichText(a_value);
		AlignRight(BridgeUI::CalcTextSize(richValue.plainText.c_str()).x);
		MCMBridge::RichTextRenderer::RenderDisabled(richValue);
	}

	bool HasReset(const MCMBridge::MCMControl& a_control)
	{
		return a_control.type != MCMBridge::MCMControlType::kEmpty &&
		       a_control.type != MCMBridge::MCMControlType::kHeader &&
		       a_control.type != MCMBridge::MCMControlType::kKeymap &&
		       a_control.type != MCMBridge::MCMControlType::kUnknown;
	}

	float ReservedResetWidth(const MCMBridge::MCMControl& a_control)
	{
		return HasReset(a_control) ? BridgeUI::GetFrameHeight() + BridgeUI::GetStyle()->ItemSpacing.x : 0.0F;
	}

	void BeginWidgetRow(std::string_view a_label, const MCMBridge::MCMControl& a_control)
	{
		if (MCMBridge::renderFrontend == MCMBridge::Frontend::kFlick) {
			MCMBridge::ControlRowRenderer::BeginRow(a_label, (std::max)(BridgeUI::GetFontSize() * 6, BridgeUI::GetContentRegionAvail().x * 0.48F), ReservedResetWidth(a_control));
			return;
		}
		const auto start = BridgeUI::GetCursorPosX();
		const auto width = BridgeUI::GetContentRegionAvail().x;
		const auto plainLabel = MCMBridge::PlainSkyUIText(a_label);
		if (!plainLabel.empty()) {
			MCMBridge::RichTextRenderer::Render(a_label);
			BridgeUI::SameLine();
		}
		const auto widgetWidth = (std::max)(140.0F, width * 0.48F);
		const auto desiredPosition = start + (std::max)(0.0F, width - widgetWidth);
		const auto widgetPosition = (std::max)(BridgeUI::GetCursorPosX(), desiredPosition);
		BridgeUI::SetCursorPosX(widgetPosition);
		BridgeUI::SetNextItemWidth((std::max)(40.0F, start + width - widgetPosition - ReservedResetWidth(a_control)));
	}

	void RenderReset(const MCMBridge::MCMSnapshot& a_snapshot, const MCMBridge::MCMControl& a_control)
	{
		if (!HasReset(a_control)) {
			return;
		}
		MCMBridge::IconButton::AlignToLastWidget(rowEnd);
		static const auto resetIcon = FontAwesome::UnicodeToUtf8(0xf0e2);
		const auto        resetID = std::format("reset-{}", a_control.identity.stableID);
		const auto        editable = MCMBridge::ControlWrite::CanReset(a_snapshot, a_control);
		BridgeUI::BeginDisabled(!editable);
		const auto clicked = MCMBridge::IconButton::Render(resetIcon, resetID);
		BridgeUI::SetItemTooltip("Reset to default");
		BridgeUI::EndDisabled();
		if (clicked && editable) {
			MCMBridge::ControlWrite::Reset(a_snapshot, a_control);
		}
	}

	void RenderTextControl(const MCMBridge::MCMSnapshot& a_snapshot, const MCMBridge::MCMControl& a_control)
	{
		const auto*            value = std::get_if<std::string>(&a_control.value);
		const std::string_view display = !a_control.displayValue.empty() ?
		                                     std::string_view(a_control.displayValue) :
		                                 value ? std::string_view(*value) :
		                                         std::string_view{};
		if (a_control.writeCapability != MCMBridge::WriteCapability::kWritable) {
			RenderValueRow(a_control.label, display);
			return;
		}

		BeginWidgetRow(a_control.label, a_control);
		const auto editable = MCMBridge::ControlWrite::IsEditable(a_snapshot, a_control);
		const auto visibleLabel = MCMBridge::PlainSkyUIText(display.empty() ? std::string_view("Activate") : display);
		const auto buttonID = std::format("{}##{}", visibleLabel, a_control.identity.stableID);
		const auto width = BridgeUI::GetContentRegionAvail().x - ReservedResetWidth(a_control);
		BridgeUI::BeginDisabled(!editable);
		if (BridgeUI::Button(buttonID.c_str(), { width, 0.0F }) && editable) {
			MCMBridge::ControlWrite::Activate(a_snapshot, a_control);
		}
		BridgeUI::EndDisabled();
		RenderReset(a_snapshot, a_control);
	}

	void RenderMenuCombo(const MCMBridge::MCMSnapshot& a_snapshot, const MCMBridge::MCMControl& a_control)
	{
		const auto* metadata = a_control.menu ? std::addressof(*a_control.menu) : nullptr;
		if (!metadata || metadata->options.empty()) {
			RenderValueRow(a_control.label, a_control.displayValue);
			return;
		}

		auto&       draft = GetDraft(a_control);
		auto        selected = std::get_if<std::int32_t>(&draft) ? std::get<std::int32_t>(draft) : metadata->selectedIndex;
		const auto  validIndex = selected >= 0 && static_cast<std::size_t>(selected) < metadata->options.size();
		const auto& options = metadata->displayOptions.empty() ? metadata->options : metadata->displayOptions;
		const auto& shortNames = metadata->displayShortNames.empty() ? metadata->shortNames : metadata->displayShortNames;
		const auto  hasShortName = validIndex && static_cast<std::size_t>(selected) < shortNames.size();
		const auto  previewSource = hasShortName ? std::string_view(shortNames[selected]) :
		                            validIndex && static_cast<std::size_t>(selected) < options.size() ?
		                                           std::string_view(options[selected]) :
		                                           std::string_view("Select");
		const auto  preview = MCMBridge::PlainSkyUIText(previewSource);
		BeginWidgetRow(a_control.label, a_control);
		const auto widgetID = std::format("##{}", a_control.identity.stableID);
		const auto editable = MCMBridge::ControlWrite::IsEditable(a_snapshot, a_control);
		BridgeUI::BeginDisabled(!editable);
		if (MCMBridge::renderFrontend == MCMBridge::Frontend::kFlick) {
			std::vector<std::string> labels;
			std::vector<const char*> items;
			labels.reserve(metadata->options.size());
			items.reserve(metadata->options.size());
			for (std::size_t index = 0; index < metadata->options.size(); ++index) {
				const auto& option = index < options.size() ? options[index] : metadata->options[index];
				labels.push_back(MCMBridge::PlainSkyUIText(option));
			}
			for (const auto& label : labels)
				items.push_back(label.c_str());
			if (BridgeUI::Combo(widgetID.c_str(), &selected, items.data(), static_cast<int>(items.size())) && editable) {
				SetDraft(a_control, selected);
				MCMBridge::ControlWrite::Submit(a_snapshot, a_control, selected);
			}
			BridgeUI::EndDisabled();
			RenderReset(a_snapshot, a_control);
			return;
		}
		if (!BridgeUI::BeginCombo(widgetID.c_str(), preview.c_str())) {
			BridgeUI::EndDisabled();
			RenderReset(a_snapshot, a_control);
			return;
		}
		for (std::size_t index = 0; index < metadata->options.size(); ++index) {
			const auto& option = index < options.size() ? options[index] : metadata->options[index];
			const auto  optionID = std::format(
				"{}##{}-{}", MCMBridge::PlainSkyUIText(option), a_control.identity.stableID, index);
			if (BridgeUI::Selectable(optionID.c_str(), static_cast<std::int32_t>(index) == selected)) {
				selected = static_cast<std::int32_t>(index);
				SetDraft(a_control, selected);
				MCMBridge::ControlWrite::Submit(a_snapshot, a_control, selected);
			}
		}
		BridgeUI::EndCombo();
		BridgeUI::EndDisabled();
		RenderReset(a_snapshot, a_control);
	}

	void RenderStepper(const MCMBridge::MCMSnapshot& a_snapshot, const MCMBridge::MCMControl& a_control)
	{
		const auto* metadata = a_control.menu ? std::addressof(*a_control.menu) : nullptr;
		const auto* selected = std::get_if<std::int32_t>(&a_control.value);
		if (!metadata || metadata->options.empty() || !selected) {
			RenderValueRow(a_control.label, a_control.displayValue);
			return;
		}
		const auto& options = metadata->displayOptions.empty() ? metadata->options : metadata->displayOptions;
		const auto  valid = *selected >= 0 && static_cast<std::size_t>(*selected) < options.size();
		const auto  display = MCMBridge::PlainSkyUIText(
			valid ? std::string_view(options[*selected]) : std::string_view(a_control.displayValue));
		BeginWidgetRow(a_control.label, a_control);
		const auto buttonID = std::format("{}##{}", display.empty() ? "Next" : display, a_control.identity.stableID);
		const auto editable = MCMBridge::ControlWrite::IsEditable(a_snapshot, a_control);
		const auto width = BridgeUI::GetContentRegionAvail().x - ReservedResetWidth(a_control);
		BridgeUI::BeginDisabled(!editable);
		if (BridgeUI::Button(buttonID.c_str(), { width, 0.0F }) && editable) {
			MCMBridge::ControlWrite::Activate(a_snapshot, a_control);
		}
		BridgeUI::EndDisabled();
		RenderReset(a_snapshot, a_control);
	}
}

namespace MCMBridge::ControlRenderer
{
	const char* BackendName(MCMBackendKind a_backend)
	{
		return a_backend == MCMBackendKind::kMCMHelper ? "MCM Helper" : "Classic SkyUI";
	}

	void Render(const MCMSnapshot& a_snapshot, const MCMControl& a_control)
	{
		if (a_control.hidden) {
			return;
		}

		const auto widgetID = std::format("##{}", a_control.identity.stableID);
		// Capture each row before label wrapping changes the cursor position.
		rowEnd = BridgeUI::GetCursorPosX() + BridgeUI::GetContentRegionAvail().x;
		BridgeUI::BeginGroup();
		switch (a_control.type) {
		case MCMControlType::kEmpty:
			break;
		case MCMControlType::kHeader:
			RichTextRenderer::RenderHeader(a_control.label);
			break;
		case MCMControlType::kText:
			RenderTextControl(a_snapshot, a_control);
			break;
		case MCMControlType::kToggle:
			{
				auto&      draft = GetDraft(a_control);
				auto       value = std::get_if<bool>(&draft) ? std::get<bool>(draft) : false;
				const auto editable = ControlWrite::IsEditable(a_snapshot, a_control);
				if (renderFrontend == Frontend::kFlick) {
					// Native checkbox icons also reserve their own trailing label gap.
					ControlRowRenderer::BeginRow(a_control.label, BridgeUI::GetFrameHeight() * 2, ReservedResetWidth(a_control));
				} else {
					const auto start = BridgeUI::GetCursorPosX();
					const auto width = BridgeUI::GetContentRegionAvail().x;
					RichTextRenderer::Render(a_control.label);
					BridgeUI::SameLine();
					const auto desiredPosition = start + (std::max)(0.0F, width - BridgeUI::GetFrameHeight() - ReservedResetWidth(a_control));
					BridgeUI::SetCursorPosX((std::max)(BridgeUI::GetCursorPosX(), desiredPosition));
				}
				BridgeUI::BeginDisabled(!editable);
				if (BridgeUI::Checkbox(widgetID.c_str(), &value)) {
					SetDraft(a_control, value);
					ControlWrite::Submit(a_snapshot, a_control, value);
				}
				BridgeUI::EndDisabled();
				RenderReset(a_snapshot, a_control);
				break;
			}
		case MCMControlType::kSlider:
			{
				auto&      draft = GetDraft(a_control);
				auto       value = std::get_if<float>(&draft) ? std::get<float>(draft) : 0.0F;
				const auto editable = ControlWrite::IsEditable(a_snapshot, a_control);
				BeginWidgetRow(a_control.label, a_control);
				BridgeUI::BeginDisabled(!editable);
				bool edited = false;
				if (a_control.slider && a_control.slider->availability == MetadataAvailability::kAvailable) {
					const auto format = MakeSliderPrintfFormat(a_control.slider->format);
					edited = BridgeUI::SliderFloat(
						widgetID.c_str(), &value, a_control.slider->minimum, a_control.slider->maximum, format.c_str());
				} else {
					edited = BridgeUI::DragFloat(widgetID.c_str(), &value, 1.0F);
				}
				const auto committed = BridgeUI::IsItemDeactivatedAfterEdit();
				BridgeUI::EndDisabled();
				if (edited) {
					SetDraft(a_control, value);
				}
				if (editable && committed) {
					ControlWrite::Submit(a_snapshot, a_control, value);
				}
				RenderReset(a_snapshot, a_control);
				break;
			}
		case MCMControlType::kMenu:
			{
				RenderMenuCombo(a_snapshot, a_control);
				break;
			}
		case MCMControlType::kStepper:
			RenderStepper(a_snapshot, a_control);
			break;
		case MCMControlType::kColor:
			{
				auto&      draft = GetDraft(a_control);
				const auto source = std::get_if<std::uint32_t>(&draft) ? std::get<std::uint32_t>(draft) : 0U;
				auto       color = UnpackARGB(source);
				const auto editable = ControlWrite::IsEditable(a_snapshot, a_control);
				BeginWidgetRow(a_control.label, a_control);
				BridgeUI::BeginDisabled(!editable);
				const auto edited = BridgeUI::ColorEdit4(widgetID.c_str(), color.data(),
					BridgeUI::ImGuiColorEditFlags_NoAlpha | BridgeUI::ImGuiColorEditFlags_DisplayHex |
						BridgeUI::ImGuiColorEditFlags_Uint8 |
						BridgeUI::ImGuiColorEditFlags_InputRGB);
				const auto committed = BridgeUI::IsItemDeactivatedAfterEdit();
				BridgeUI::EndDisabled();
				if (edited) {
					SetDraft(a_control, PackARGB(color));
				}
				if (editable && committed) {
					ControlWrite::Submit(a_snapshot, a_control, PackARGB(color));
				}
				RenderReset(a_snapshot, a_control);
				break;
			}
		case MCMControlType::kKeymap:
			{
				auto&      draft = GetDraft(a_control);
				const auto selected = std::get_if<std::int32_t>(&draft) ? std::get<std::int32_t>(draft) : -1;
				const auto editable = ControlWrite::IsEditable(a_snapshot, a_control);
				BridgeUI::BeginDisabled(!editable);
				const auto changed = KeybindSelector::Render(
					a_control.identity.stableID, a_control.label, selected, editable, a_control.allowUnmap);
				BridgeUI::EndDisabled();
				if (changed) {
					if (changed->action == KeybindSelector::Action::kReset) {
						ControlWrite::Reset(a_snapshot, a_control);
					} else {
						SetDraft(a_control, changed->value);
						ControlWrite::Submit(a_snapshot, a_control, changed->value);
					}
				}
				break;
			}
		case MCMControlType::kInput:
			{
				const auto editable = ControlWrite::IsEditable(a_snapshot, a_control);
				BeginWidgetRow(a_control.label, a_control);
				if (auto value = InputSelector::Render(a_control, editable)) {
					ControlWrite::Submit(a_snapshot, a_control, std::move(*value));
				}
				RenderReset(a_snapshot, a_control);
				break;
			}
		default:
			RenderValueRow(a_control.label, "Unsupported");
			break;
		}
		BridgeUI::EndGroup();
		ControlHelp::Render(a_control);
	}
}
