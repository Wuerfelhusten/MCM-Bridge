#include "MCMBridge/Core/ControlRowLayout.h"
#include "MCMBridge/UI/FlickWidgetDecoration.h"
#include "MCMBridge/UI/FrontendUI.h"

// The SMF header must declare its own value types before global ImGui types.
#include "FUCK_API.h"
#include "imgui_internal.h"

#include <cstdarg>
#include <cstdio>

namespace MCMBridge::FrontendUI
{
	static_assert(static_cast<int>(ImGuiMCP::ImGuiTableFlags_SizingStretchProp) == ::ImGuiTableFlags_SizingStretchProp);
	static_assert(static_cast<int>(ImGuiMCP::ImGuiTableColumnFlags_WidthFixed) == ::ImGuiTableColumnFlags_WidthFixed);
	static_assert(static_cast<int>(ImGuiMCP::ImGuiTableColumnFlags_WidthStretch) == ::ImGuiTableColumnFlags_WidthStretch);
	namespace
	{
		bool             Flick() { return renderFrontend == Frontend::kFlick; }
		thread_local int comboDepth{};
		FUCK_Interface&  Api() { return *FUCK::GetInterface(); }
		std::string      Format(const char* a_format, va_list a_arguments)
		{
			va_list copy;
			va_copy(copy, a_arguments);
			const auto size = std::vsnprintf(nullptr, 0, a_format, copy);
			va_end(copy);
			if (size < 0)
				return {};
			std::string result(static_cast<std::size_t>(size) + 1, '\0');
			std::vsnprintf(result.data(), result.size(), a_format, a_arguments);
			result.resize(static_cast<std::size_t>(size));
			return result;
		}
	}

	bool Button(const char* a_label, ImVec2 a_size)
	{
		if (!Flick())
			return ImGuiMCP::Button(a_label, a_size);
		const std::string_view source(a_label);
		const auto             hidden = source.find("##");
		const auto             visible = source.substr(0, hidden);
		const auto             identity = hidden == std::string_view::npos ? source : source.substr(hidden + 2);
		const auto             text = CalcTextSize(std::string(visible).c_str());
		const auto             padding = GetStyle()->FramePadding.x;
		const auto             width = (std::max)(1.0F, (std::min)(GetContentRegionAvail().x, a_size.x > 0 ? a_size.x : text.x + 2 * padding));
		const auto             height = a_size.y > 0 ? a_size.y : GetFrameHeight();
		const auto             shortened = FitControlText(visible, width - 2 * padding, [](std::string_view a_text) { return CalcTextSize(std::string(a_text).c_str()).x; });
		const auto             label = std::format("{}###{}", shortened, identity);
		// OutlineButton has no size argument and measures the hidden ID too.
		// Selectables normally expand by half ItemSpacing on all sides. Disable
		// that list-row behavior for bounded buttons, including the last column.
		Api().PushStyleVarVec(::ImGuiStyleVar_SelectableTextAlign, { 0.5F, 0.5F });
		const auto pressed = Api().Selectable(label.c_str(), false, ::ImGuiSelectableFlags_NoPadWithHalfSpacing, { width, height });
		Api().PopStyleVar(1);
		::ImVec2 minimum, maximum;
		Api().GetItemRectMin(&minimum.x, &minimum.y);
		Api().GetItemRectMax(&maximum.x, &maximum.y);
		::ImVec4 color;
		Api().GetStyleColorVec4(Api().IsItemHovered(0) ? ::ImGuiCol_Text : ::ImGuiCol_Border, &color.x, &color.y, &color.z, &color.w);
		color.w *= Api().GetStyleVar(::ImGuiStyleVar_Alpha);
		Api().DrawRect({ minimum.x + 0.5F, minimum.y + 0.5F }, { maximum.x - 0.5F, maximum.y - 0.5F }, color, 2, 1);
		if (shortened != visible && Api().IsItemHovered(0))
			Api().SetTooltip(std::string(visible).c_str());
		return pressed;
	}
	bool Checkbox(const char* a_label, bool* a_value) { return Flick() ? Api().Checkbox(a_label, a_value, false, false) : ImGuiMCP::Checkbox(a_label, a_value); }
	bool InputText(const char* a_label, char* a_buffer, std::size_t a_size, int a_flags) { return Flick() ? Api().InputText(a_label, a_buffer, a_size, (a_flags & ImGuiMCP::ImGuiInputTextFlags_EnterReturnsTrue) ? ::ImGuiInputTextFlags_EnterReturnsTrue : 0) : ImGuiMCP::InputText(a_label, a_buffer, a_size, a_flags); }
	bool SliderFloat(const char* a_label, float* a_value, float a_min, float a_max, const char* a_format)
	{
		if (!Flick())
			return ImGuiMCP::SliderFloat(a_label, a_value, a_min, a_max, a_format);
		return Api().SliderFloat(a_label, a_value, a_min, a_max, a_format);
	}
	bool SliderInt(const char* a_label, int* a_value, int a_min, int a_max, const char* a_format)
	{
		if (!Flick())
			return ImGuiMCP::SliderInt(a_label, a_value, a_min, a_max, a_format);
		return Api().SliderInt(a_label, a_value, a_min, a_max, a_format);
	}
	bool DragFloat(const char* a_label, float* a_value, float a_speed, float a_min, float a_max, const char* a_format) { return Flick() ? Api().DragFloat(a_label, a_value, a_speed, a_min, a_max, a_format) : ImGuiMCP::DragFloat(a_label, a_value, a_speed, a_min, a_max, a_format); }
	bool ColorEdit4(const char* a_label, float* a_color, int a_flags)
	{
		int flags{};
		if (a_flags & ImGuiMCP::ImGuiColorEditFlags_DisplayHex)
			flags |= ::ImGuiColorEditFlags_DisplayHex;
		if (a_flags & ImGuiMCP::ImGuiColorEditFlags_InputRGB)
			flags |= ::ImGuiColorEditFlags_InputRGB;
		if (a_flags & ImGuiMCP::ImGuiColorEditFlags_NoAlpha)
			flags |= ::ImGuiColorEditFlags_NoAlpha;
		if (a_flags & ImGuiMCP::ImGuiColorEditFlags_Uint8)
			flags |= ::ImGuiColorEditFlags_Uint8;
		return Flick() ? Api().ColorEdit4(a_label, a_color, flags) : ImGuiMCP::ColorEdit4(a_label, a_color, a_flags);
	}
	bool Selectable(const char* a_label, bool a_selected)
	{
		const auto selected = Flick() ? Api().Selectable(a_label, a_selected, 0, {}) : ImGuiMCP::Selectable(a_label, a_selected);
		if (Flick() && selected && comboDepth)
			Api().CloseCurrentPopup();
		return selected;
	}
	bool BeginCombo(const char* a_id, const char* a_preview)
	{
		if (!Flick())
			return ImGuiMCP::BeginCombo(a_id, a_preview);
		const auto label = std::format("{}##{}", a_preview, a_id);
		if (Button(label.c_str(), { Api().CalcItemWidth(), 0 }))
			Api().OpenPopup(a_id, 0);
		FlickWidgetDecoration::Dropdown();
		const auto open = Api().BeginPopup(a_id, 0);
		if (open)
			++comboDepth;
		return open;
	}
	bool Combo(const char* a_label, int* a_selected, const char* const* a_items, int a_count)
	{
		if (!Flick())
			return ImGuiMCP::Combo(a_label, a_selected, a_items, a_count);
		// FLICK treats ## as hidden IDs and ##HEADER: as structural data. These
		// are display labels only; the original option index remains unchanged.
		std::vector<std::string> labels;
		std::vector<const char*> items;
		labels.reserve(static_cast<std::size_t>((std::max)(0, a_count)));
		for (int index = 0; index < a_count; ++index) {
			std::string label(a_items[index]);
			std::size_t position{};
			while ((position = label.find("##", position)) != std::string::npos) {
				label.insert(position + 1, " ");
				position += 2;
			}
			labels.push_back(std::move(label));
		}
		for (const auto& label : labels)
			items.push_back(label.c_str());
		// An open FLICK combo leaves its popup's last item behind. Publish the
		// parent widget's group bounds instead, so trailing actions stay aligned.
		Api().BeginGroup();
		const auto changed = Api().Combo(a_label, a_selected, items.data(), a_count);
		Api().EndGroup();
		return changed;
	}
	void EndCombo()
	{
		if (Flick()) {
			--comboDepth;
			Api().EndPopup();
		} else
			ImGuiMCP::EndCombo();
	}
	bool TreeNode(const char* a_label) { return Flick() ? Api().TreeNode(a_label) : ImGuiMCP::TreeNode(a_label); }
	void TreePop()
	{
		if (Flick())
			Api().TreePop();
		else
			ImGuiMCP::TreePop();
	}
	bool CollapsingHeader(const char* a_label) { return Flick() ? Api().CollapsingHeader(a_label, 0) : ImGuiMCP::CollapsingHeader(a_label); }
	bool BeginTable(const char* a_id, int a_columns, int a_flags) { return Flick() ? Api().BeginTable(a_id, a_columns, a_flags, {}, 0) : ImGuiMCP::BeginTable(a_id, a_columns, a_flags); }
	void EndTable()
	{
		if (Flick())
			Api().EndTable();
		else
			ImGuiMCP::EndTable();
	}
	void TableSetupColumn(const char* a_label, int a_flags, float a_width)
	{
		if (Flick())
			Api().TableSetupColumn(a_label, a_flags, a_width, 0);
		else
			ImGuiMCP::TableSetupColumn(a_label, a_flags, a_width);
	}
	void TableNextRow(int a_flags, float a_height)
	{
		if (Flick())
			Api().TableNextRow(a_flags, a_height);
		else
			ImGuiMCP::TableNextRow(a_flags, a_height);
	}
	void TableSetColumnIndex(int a_index)
	{
		if (Flick())
			Api().TableSetColumnIndex(a_index);
		else
			ImGuiMCP::TableSetColumnIndex(a_index);
	}
	bool IsItemHovered() { return Flick() ? Api().IsItemHovered(0) : ImGuiMCP::IsItemHovered(); }
	bool IsItemDeactivatedAfterEdit() { return Flick() ? Api().IsItemDeactivatedAfterEdit() : ImGuiMCP::IsItemDeactivatedAfterEdit(); }
	void SetItemDefaultFocus()
	{
		if (Flick())
			Api().SetItemDefaultFocus();
		else
			ImGuiMCP::SetItemDefaultFocus();
	}
	void TextUnformatted(const char* a_text, const char* a_end)
	{
		if (Flick())
			Api().TextUnformatted(a_text, a_end);
		else
			ImGuiMCP::TextUnformatted(a_text, a_end);
	}
	void Text(const char* a_format, ...)
	{
		va_list arguments;
		va_start(arguments, a_format);
		const auto text = Format(a_format, arguments);
		va_end(arguments);
		TextUnformatted(text.c_str());
	}
	void TextWrapped(const char* a_format, ...)
	{
		va_list arguments;
		va_start(arguments, a_format);
		const auto text = Format(a_format, arguments);
		va_end(arguments);
		if (Flick())
			Api().TextWrapped(text.c_str());
		else
			ImGuiMCP::TextWrapped("%s", text.c_str());
	}
	void TextDisabled(const char* a_format, ...)
	{
		va_list arguments;
		va_start(arguments, a_format);
		const auto text = Format(a_format, arguments);
		va_end(arguments);
		if (Flick())
			Api().TextDisabled(text.c_str());
		else
			ImGuiMCP::TextDisabled("%s", text.c_str());
	}
	void SetItemTooltip(const char* a_format, ...)
	{
		va_list arguments;
		va_start(arguments, a_format);
		const auto text = Format(a_format, arguments);
		va_end(arguments);
		if (Flick()) {
			if (Api().IsItemHovered(0))
				Api().SetTooltip(text.c_str());
		} else
			ImGuiMCP::SetItemTooltip("%s", text.c_str());
	}
}
