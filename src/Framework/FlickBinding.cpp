#include "FUCK_API.h"
#include "MCMBridge/Framework/FlickApi.h"

#include <bit>

namespace MCMBridge::FlickApi
{
	bool Bind()
	{
		const auto module = GetModuleHandleW(L"FUCK.dll");
		if (!module)
			return false;
		const auto address = GetProcAddress(module, "RequestFUCK");
		if (!address)
			return false;
		const auto request = std::bit_cast<void* (*)()>(address);
		auto*      api = static_cast<FUCK_Interface*>(request());
		if (!api || api->version < FUCK_API_VERSION) {
			SKSE::log::error("FLICK API {} is unavailable; required API {}", api ? api->version : 0, FUCK_API_VERSION);
			return false;
		}
		// Check the whole used prefix before publishing the table. A loaded DLL
		// is not sufficient evidence that drawing and input are available.
		const std::pair<const char*, bool> capabilities[] = {
			{ "AddImage", api->AddImage != nullptr },
			{ "BeginChild", api->BeginChild != nullptr },
			{ "BeginDisabled", api->BeginDisabled != nullptr },
			{ "BeginGroup", api->BeginGroup != nullptr },
			{ "BeginPopup", api->BeginPopup != nullptr },
			{ "BeginTable", api->BeginTable != nullptr },
			{ "BeginWindow", api->BeginWindow != nullptr },
			{ "Button", api->Button != nullptr },
			{ "CalcTextSize", api->CalcTextSize != nullptr },
			{ "Checkbox", api->Checkbox != nullptr },
			{ "CloseCurrentPopup", api->CloseCurrentPopup != nullptr },
			{ "CollapsingHeader", api->CollapsingHeader != nullptr },
			{ "ColorEdit4", api->ColorEdit4 != nullptr },
			{ "Combo", api->Combo != nullptr },
			{ "DragFloat", api->DragFloat != nullptr },
			{ "DrawRect", api->DrawRect != nullptr },
			{ "DrawLine", api->DrawLine != nullptr },
			{ "DrawTriangleFilled", api->DrawTriangleFilled != nullptr },
			{ "Dummy", api->Dummy != nullptr },
			{ "EndChild", api->EndChild != nullptr },
			{ "EndDisabled", api->EndDisabled != nullptr },
			{ "EndGroup", api->EndGroup != nullptr },
			{ "EndPopup", api->EndPopup != nullptr },
			{ "EndTable", api->EndTable != nullptr },
			{ "EndWindow", api->EndWindow != nullptr },
			{ "GetContentRegionAvail", api->GetContentRegionAvail != nullptr },
			{ "GetCursorPos", api->GetCursorPos != nullptr },
			{ "GetCursorScreenPos", api->GetCursorScreenPos != nullptr },
			{ "GetDisplaySize", api->GetDisplaySize != nullptr },
			{ "GetFrameHeight", api->GetFrameHeight != nullptr },
			{ "GetFont", api->GetFont != nullptr },
			{ "GetFrameHeightWithSpacing", api->GetFrameHeightWithSpacing != nullptr },
			{ "GetItemRectMin", api->GetItemRectMin != nullptr },
			{ "GetItemRectMax", api->GetItemRectMax != nullptr },
			{ "GetImageInfo", api->GetImageInfo != nullptr },
			{ "GetResolutionScale", api->GetResolutionScale != nullptr },
			{ "GetStyleColorVec4", api->GetStyleColorVec4 != nullptr },
			{ "GetStyleVarVec", api->GetStyleVarVec != nullptr },
			{ "GetStyleVar", api->GetStyleVar != nullptr },
			{ "GetTextLineHeight", api->GetTextLineHeight != nullptr },
			{ "GetTextLineHeightWithSpacing", api->GetTextLineHeightWithSpacing != nullptr },
			{ "InputText", api->InputText != nullptr },
			{ "InvisibleButton", api->InvisibleButton != nullptr },
			{ "Indent", api->Indent != nullptr },
			{ "CalcItemWidth", api->CalcItemWidth != nullptr },
			{ "IsItemDeactivatedAfterEdit", api->IsItemDeactivatedAfterEdit != nullptr },
			{ "IsItemHovered", api->IsItemHovered != nullptr },
			{ "IsMenuOpen", api->IsMenuOpen != nullptr },
			{ "LoadTranslation", api->LoadTranslation != nullptr },
			{ "LoadImage", api->LoadImage != nullptr },
			{ "OpenPopup", api->OpenPopup != nullptr },
			{ "PopID", api->PopID != nullptr },
			{ "PopFont", api->PopFont != nullptr },
			{ "PopStyleColor", api->PopStyleColor != nullptr },
			{ "PopStyleVar", api->PopStyleVar != nullptr },
			{ "PopTextWrapPos", api->PopTextWrapPos != nullptr },
			{ "PushID_Int", api->PushID_Int != nullptr },
			{ "PushFont", api->PushFont != nullptr },
			{ "PushID_Str", api->PushID_Str != nullptr },
			{ "PushStyleColor", api->PushStyleColor != nullptr },
			{ "PushStyleVarVec", api->PushStyleVarVec != nullptr },
			{ "PushTextWrapPos", api->PushTextWrapPos != nullptr },
			{ "RegisterTool", api->RegisterTool != nullptr },
			{ "RegisterWindow", api->RegisterWindow != nullptr },
			{ "ReleaseImage", api->ReleaseImage != nullptr },
			{ "SameLine", api->SameLine != nullptr },
			{ "Selectable", api->Selectable != nullptr },
			{ "Separator", api->Separator != nullptr },
			{ "SetCursorPosX", api->SetCursorPosX != nullptr },
			{ "SetCursorPos", api->SetCursorPos != nullptr },
			{ "SetItemDefaultFocus", api->SetItemDefaultFocus != nullptr },
			{ "SetMenuOpen", api->SetMenuOpen != nullptr },
			{ "SetNextItemWidth", api->SetNextItemWidth != nullptr },
			{ "SetNextWindowPos", api->SetNextWindowPos != nullptr },
			{ "SetNextWindowSize", api->SetNextWindowSize != nullptr },
			{ "SetTooltip", api->SetTooltip != nullptr },
			{ "SetWindowFocus", api->SetWindowFocus != nullptr },
			{ "SliderFloat", api->SliderFloat != nullptr },
			{ "SliderInt", api->SliderInt != nullptr },
			{ "Spacing", api->Spacing != nullptr },
			{ "TableNextRow", api->TableNextRow != nullptr },
			{ "TableSetColumnIndex", api->TableSetColumnIndex != nullptr },
			{ "TableSetupColumn", api->TableSetupColumn != nullptr },
			{ "TextDisabled", api->TextDisabled != nullptr },
			{ "TextUnformatted", api->TextUnformatted != nullptr },
			{ "TextWrapped", api->TextWrapped != nullptr },
			{ "TreeNode", api->TreeNode != nullptr },
			{ "TreePop", api->TreePop != nullptr },
			{ "Unindent", api->Unindent != nullptr },
		};
		for (const auto& [name, available] : capabilities) {
			if (!available) {
				SKSE::log::error("FLICK API is missing required capability {}", name);
				return false;
			}
		}
		FUCK::GetInterface() = api;
		FUCK::g_pluginName = "MCMBridge";
		api->LoadTranslation("MCMBridge");
		SKSE::log::info("Connected FLICK API {}", api->version);
		return true;
	}
}
