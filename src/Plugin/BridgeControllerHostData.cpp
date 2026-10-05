#include "MCMBridge/Plugin/BridgeController.h"

namespace MCMBridge
{
	Result<HostDataInput> BridgeController::ReadHostData(MCMHostContext a_context)
	{
		if (!IsNativeHost() || !IsSessionReady())
			return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "Native host is not ready" });
		if (a_context && (directContext.id != a_context || directContext.ended || directContext.cancelled))
			return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "Native host context is not current" });
		if (a_context && directContext.calls && directContext.calls->Busy())
			return std::unexpected(BridgeError{ BridgeErrorCode::kBusy, "Native host callback has not completed" });
		auto entries = registry.Native().ReadLive();
		if (!entries)
			return std::unexpected(entries.error());
		HostDataInput data;
		data.session = session;
		data.registry.reserve(entries->size());
		for (const auto& entry : *entries)
			data.registry.push_back(entry.descriptor);
		const auto script = a_context ? directContext.script : nullptr;
		if (!script || !script->IsConfigOpen())
			return data;
		const auto navigation = script->ReadNavigationPages();
		if (!navigation)
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Native navigation is unavailable" });
		const auto entry = std::ranges::find_if(data.registry, [this](const auto& a_entry) { return a_entry.stableID == directContext.stableID; });
		if (entry == data.registry.end())
			return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "Native registration was removed" });
		MCMMod mod;
		mod.stableID = entry->stableID;
		mod.interopID = entry->interopID;
		mod.displayName = entry->displayName;
		mod.ownerPlugin = entry->ownerPlugin;
		mod.scriptName = entry->scriptName;
		mod.questFormID = entry->questFormID;
		mod.backend = entry->backend;
		mod.pageScopedState = entry->pageScopedState;
		for (std::size_t i = 0; i < navigation->size(); ++i) {
			MCMPage page;
			page.index = static_cast<std::int32_t>(i);
			page.rawName = (*navigation)[i];
			mod.pages.push_back(std::move(page));
		}
		if (const auto selected = script->ReadCurrentPage(); selected && script->IsPageReady(selected->index)) {
			auto page = script->ReadPage({ mod.stableID, mod.ownerPlugin, mod.questFormID, mod.scriptName, selected->name, selected->index });
			if (!page)
				return std::unexpected(page.error());
			// Only the explicitly prepared dialog can be exposed as current. Reading
			// metadata here never dispatches another Papyrus call.
			if (directContext.lastCall) {
				const auto& call = *directContext.lastCall;
				for (auto& control : page->controls) {
					if (control.identity.optionIndex != call.integer)
						continue;
					const auto index = control.identity.optionIndex;
					if (call.method == ClassicMethod::kRequestSliderDialogData) {
						if (auto value = script->ReadSliderMetadata(index))
							control.slider = std::move(*value);
					} else if (call.method == ClassicMethod::kRequestMenuDialogData) {
						if (auto value = script->ReadMenuMetadata(index))
							control.menu = std::move(*value);
					} else if (call.method == ClassicMethod::kRequestColorDialogData) {
						if (auto value = script->ReadColorMetadata(index))
							control.color = std::move(*value);
					} else if (call.method == ClassicMethod::kRequestInputDialogData) {
						if (auto value = script->ReadInputMetadata(index))
							control.input = std::move(*value);
					}
				}
			}
			if (selected->index == -1)
				mod.pages.insert(mod.pages.begin(), std::move(*page));
			else if (selected->index >= 0 && static_cast<std::size_t>(selected->index) < mod.pages.size())
				mod.pages[static_cast<std::size_t>(selected->index)] = std::move(*page);
			else
				return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Native page is outside current navigation" });
			data.currentPage = selected->index;
		}
		data.active = std::move(mod);
		return data;
	}
}
