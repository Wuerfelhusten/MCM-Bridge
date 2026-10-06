#include "MCMBridge/UI/FrontendUI.h"
#include "FUCK_API.h"

namespace MCMBridge::FrontendUI
{
	// Only these layout flags cross as integers. Check both header definitions
	// rather than assuming that different ImGui releases share every enum.
	static_assert(static_cast<int>(ImGuiMCP::ImGuiCond_Always) == ::ImGuiCond_Always);
	static_assert(static_cast<int>(ImGuiMCP::ImGuiCond_FirstUseEver) == ::ImGuiCond_FirstUseEver);
	static_assert(static_cast<int>(ImGuiMCP::ImGuiWindowFlags_AlwaysAutoResize) == ::ImGuiWindowFlags_AlwaysAutoResize);
	namespace
	{
		bool            Flick() { return renderFrontend == Frontend::kFlick; }
		FUCK_Interface& Api() { return *FUCK::GetInterface(); }
		::ImVec2        Vector(ImVec2 a_value) { return { a_value.x, a_value.y }; }
		::ImVec4        Vector(ImVec4 a_value) { return { a_value.x, a_value.y, a_value.z, a_value.w }; }
	}

	const Style* GetStyle()
	{
		thread_local Style style;
		if (Flick()) {
			Api().GetStyleVarVec(::ImGuiStyleVar_FramePadding, &style.FramePadding.x, &style.FramePadding.y);
			Api().GetStyleVarVec(::ImGuiStyleVar_ItemSpacing, &style.ItemSpacing.x, &style.ItemSpacing.y);
		} else {
			style = { ImGuiMCP::GetStyle()->FramePadding, ImGuiMCP::GetStyle()->ItemSpacing };
		}
		return &style;
	}
	ImVec2 DisplayCenter()
	{
		if (Flick()) {
			ImVec2 size;
			Api().GetDisplaySize(&size.x, &size.y);
			return { size.x * 0.5F, size.y * 0.5F };
		}
		const auto* viewport = ImGuiMCP::GetMainViewport();
		return { viewport->WorkPos.x + viewport->WorkSize.x * 0.5F, viewport->WorkPos.y + viewport->WorkSize.y * 0.5F };
	}
	ImVec2 GetContentRegionAvail()
	{
		if (!Flick())
			return ImGuiMCP::GetContentRegionAvail();
		ImVec2 value;
		Api().GetContentRegionAvail(&value.x, &value.y);
		return value;
	}
	ImVec2 CalcTextSize(const char* a_text)
	{
		if (!Flick())
			return ImGuiMCP::CalcTextSize(a_text);
		ImVec2 value;
		Api().CalcTextSize(a_text, nullptr, false, -1, &value.x, &value.y);
		return value;
	}
	float GetCursorPosX()
	{
		if (!Flick())
			return ImGuiMCP::GetCursorPosX();
		float x, y;
		Api().GetCursorPos(&x, &y);
		return x;
	}
	float GetFontSize() { return Flick() ? Api().GetTextLineHeight() : ImGuiMCP::GetFontSize(); }
	float GetFrameHeight() { return Flick() ? Api().GetFrameHeight() : ImGuiMCP::GetFrameHeight(); }
	float GetFrameHeightWithSpacing() { return Flick() ? Api().GetFrameHeightWithSpacing() : ImGuiMCP::GetFrameHeightWithSpacing(); }
	float GetTextLineHeightWithSpacing() { return Flick() ? Api().GetTextLineHeightWithSpacing() : ImGuiMCP::GetTextLineHeightWithSpacing(); }
	void  SetCursorPosX(float a_x)
	{
		if (Flick())
			Api().SetCursorPosX(a_x);
		else
			ImGuiMCP::SetCursorPosX(a_x);
	}
	void SetNextItemWidth(float a_width)
	{
		if (Flick())
			Api().SetNextItemWidth(a_width);
		else
			ImGuiMCP::SetNextItemWidth(a_width);
	}
	void SetNextWindowSize(ImVec2 a_size, int a_condition)
	{
		if (Flick()) {
			if (!hostedWindowDraw)
				Api().SetNextWindowSize(a_size.x, a_size.y, a_condition);
		} else
			ImGuiMCP::SetNextWindowSize(a_size, a_condition);
	}
	void SetNextWindowPos(ImVec2 a_position, int a_condition, ImVec2 a_pivot)
	{
		if (Flick()) {
			if (!hostedWindowDraw)
				Api().SetNextWindowPos(a_position.x, a_position.y, a_condition, a_pivot.x, a_pivot.y);
		} else
			ImGuiMCP::SetNextWindowPos(a_position, a_condition, a_pivot);
	}
	void SetNextWindowFocus()
	{
		if (Flick())
			Api().SetWindowFocus();
		else
			ImGuiMCP::SetNextWindowFocus();
	}
	bool Begin(const char* a_title, bool* a_open, int a_flags) { return Flick() ? (hostedWindowDraw || Api().BeginWindow(a_title, a_open, a_flags)) : ImGuiMCP::Begin(a_title, a_open, a_flags); }
	void End()
	{
		if (Flick()) {
			if (!hostedWindowDraw)
				Api().EndWindow();
		} else
			ImGuiMCP::End();
	}
	bool BeginChild(const char* a_id, ImVec2 a_size, int a_flags)
	{
		if (!Flick())
			return ImGuiMCP::BeginChild(a_id, a_size, a_flags);
		Api().BeginChild(a_id, a_size.x, a_size.y, (a_flags & ImGuiMCP::ImGuiChildFlags_Border) != 0, 0);
		return true;
	}
	void EndChild()
	{
		if (Flick())
			Api().EndChild();
		else
			ImGuiMCP::EndChild();
	}
	void BeginGroup()
	{
		if (Flick())
			Api().BeginGroup();
		else
			ImGuiMCP::BeginGroup();
	}
	void EndGroup()
	{
		if (Flick())
			Api().EndGroup();
		else
			ImGuiMCP::EndGroup();
	}
	void BeginDisabled(bool a_disabled)
	{
		if (Flick())
			Api().BeginDisabled(a_disabled);
		else
			ImGuiMCP::BeginDisabled(a_disabled);
	}
	void EndDisabled()
	{
		if (Flick())
			Api().EndDisabled();
		else
			ImGuiMCP::EndDisabled();
	}
	void SameLine(float a_offset, float a_spacing)
	{
		if (Flick())
			Api().SameLine(a_offset, a_spacing);
		else
			ImGuiMCP::SameLine(a_offset, a_spacing);
	}
	void Separator()
	{
		if (Flick())
			Api().Separator();
		else
			ImGuiMCP::Separator();
	}
	void Spacing()
	{
		if (Flick())
			Api().Spacing();
		else
			ImGuiMCP::Spacing();
	}
	void PushID(int a_id)
	{
		if (Flick())
			Api().PushID_Int(a_id);
		else
			ImGuiMCP::PushID(a_id);
	}
	void PopID()
	{
		if (Flick())
			Api().PopID();
		else
			ImGuiMCP::PopID();
	}
	void PushStyleColor(int a_color, ImVec4 a_value)
	{
		if (Flick())
			Api().PushStyleColor(a_color == ImGuiMCP::ImGuiCol_TextDisabled ? ::ImGuiCol_TextDisabled : ::ImGuiCol_Text, Vector(a_value));
		else
			ImGuiMCP::PushStyleColor(a_color, a_value);
	}
	void PopStyleColor(int a_count)
	{
		if (Flick())
			Api().PopStyleColor(a_count);
		else
			ImGuiMCP::PopStyleColor(a_count);
	}
	void PushStyleVar(int a_var, ImVec2 a_value)
	{
		if (Flick())
			Api().PushStyleVarVec(a_var == ImGuiMCP::ImGuiStyleVar_FramePadding ? ::ImGuiStyleVar_FramePadding : ::ImGuiStyleVar_ButtonTextAlign, Vector(a_value));
		else
			ImGuiMCP::PushStyleVar(a_var, a_value);
	}
	void PopStyleVar(int a_count)
	{
		if (Flick())
			Api().PopStyleVar(a_count);
		else
			ImGuiMCP::PopStyleVar(a_count);
	}
	void PushTextWrapPos(float a_position)
	{
		if (Flick())
			Api().PushTextWrapPos(a_position);
		else
			ImGuiMCP::PushTextWrapPos(a_position);
	}
	void PopTextWrapPos()
	{
		if (Flick())
			Api().PopTextWrapPos();
		else
			ImGuiMCP::PopTextWrapPos();
	}
}
