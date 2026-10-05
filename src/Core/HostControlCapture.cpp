#include "MCMBridge/Core/HostControlCapture.h"

#include <cmath>
#include <utility>

namespace MCMBridge
{
	bool HostControlCapture::Observe(HostProtocolCallKind a_kind, std::string_view a_target, const HostBufferPayload& a_payload)
	{
		const auto assign = [&]<class T>(std::optional<T>& a_destination) {
			if (const auto* value = std::get_if<T>(&a_payload))
				a_destination = *value;
			else
				changes.malformed = true;
		};
		if (a_kind == HostProtocolCallKind::kSet) {
			if (a_target == "optionCursorIndex") {
				cursor.reset();
				assign(cursor);
				if (cursor && *cursor < 0) {
					changes.malformed = true;
					cursor.reset();
				}
			} else if (a_target == "optionCursor.numValue" || a_target == "optionCursor.strValue") {
				if (!cursor) {
					changes.malformed = true;
					return true;
				}
				HostControlChange change{ .index = *cursor };
				if (a_target == "optionCursor.strValue") {
					assign(change.text);
				} else {
					if (const auto* integer = std::get_if<std::int32_t>(&a_payload))
						change.number = static_cast<float>(*integer);
					else
						assign(change.number);
					if (change.number && !std::isfinite(*change.number))
						changes.malformed = true;
				}
				changes.controls.push_back(std::move(change));
			} else {
				return false;
			}
		} else if (a_target == "flushOptionBuffers") {
			// A new page supersedes deltas emitted before its buffer flush.
			changes.controls.clear();
			cursor.reset();
		} else if (a_target == "setOptionFlags") {
			const auto* values = std::get_if<std::vector<std::int32_t>>(&a_payload);
			if (!values || values->size() != 2 || (*values)[0] < 0)
				changes.malformed = true;
			else
				changes.controls.push_back({ .index = (*values)[0], .flags = (*values)[1] });
		} else if (a_target == "invalidateOptionData") {
			changes.redrawRequested = true;
		} else if (a_target == "forcePageReset") {
			changes.resetRequested = true;
		} else if (a_target == "setPageNames") {
			assign(changes.navigation);
		} else if (a_target == "setMenuDialogOptions") {
			assign(changes.menuOptions);
		} else if (a_target == "setSliderDialogParams") {
			assign(changes.sliderParameters);
		} else if (a_target == "setMenuDialogParams") {
			assign(changes.menuParameters);
		} else if (a_target == "setColorDialogParams") {
			assign(changes.colorParameters);
		} else if (a_target == "setInputDialogParams") {
			assign(changes.inputText);
		} else if (a_target == "setTitleText") {
			assign(changes.title);
		} else if (a_target == "setInfoText") {
			assign(changes.info);
		} else {
			return false;
		}
		return true;
	}

	HostControlChanges HostControlCapture::Complete()
	{
		cursor.reset();
		return std::exchange(changes, {});
	}
}
