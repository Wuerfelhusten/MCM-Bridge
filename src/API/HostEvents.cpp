#include "MCMBridge/API/HostEvents.h"
#include "MCMBridge/API/HostData.h"

namespace MCMBridge
{
	HostEvents& HostEvents::GetSingleton()
	{
		static HostEvents instance;
		return instance;
	}

	MCMHostResult HostEvents::Subscribe(MCMHostObserver a_observer, void* a_user, std::uint64_t& a_subscription)
	{
		a_subscription = 0;
		if (!a_observer)
			return MCM_HOST_INVALID_ARGUMENT;
		const std::scoped_lock lock(mutex);
		if (nextID == 0)
			return MCM_HOST_UNAVAILABLE;
		const auto id = nextID++;
		observers.emplace(id, Observer{ a_observer, a_user });
		a_subscription = id;
		return MCM_HOST_OK;
	}

	MCMHostResult HostEvents::Unsubscribe(std::uint64_t a_subscription)
	{
		const std::scoped_lock lock(mutex);
		return observers.erase(a_subscription) ? MCM_HOST_OK : MCM_HOST_INVALID_ARGUMENT;
	}

	void HostEvents::Publish(const MCMHostEvent& a_event)
	{
		const std::scoped_lock     lock(mutex);
		std::vector<std::uint64_t> ids;
		ids.reserve(observers.size());
		for (const auto& [id, observer] : observers) {
			static_cast<void>(observer);
			ids.push_back(id);
		}
		for (const auto id : ids) {
			const auto found = observers.find(id);
			if (found == observers.end())
				continue;
			const auto observer = found->second;
			try {
				observer.callback(observer.user, &a_event);
			} catch (...) {
				// A client cannot prevent other receivers from observing the change.
			}
		}
	}

	void HostEvents::Session(std::uint32_t a_type, std::uint64_t a_session)
	{
		MCMHostEvent event{};
		event.type = a_type;
		event.session = a_session;
		Publish(event);
	}

	void HostEvents::Changed(std::uint64_t a_session, const MCMMod& a_mod, const MCMPage& a_page, const MCMControl& a_control, const MCMValue& a_value)
	{
		MCMHostEvent event{};
		event.type = MCM_HOST_USER_CHANGE;
		event.session = a_session;
		PublishControl(event, a_mod, a_page, a_control, a_value);
	}

	void HostEvents::Write(std::uint32_t a_type, std::uint64_t a_session, const WriteCommand& a_command, const MCMValue& a_value, bool a_accepted, bool a_declined)
	{
		if (!a_command.recordingSnapshot || !a_command.recordingID)
			return;
		MCMHostEvent event{};
		event.type = a_type;
		event.session = a_session;
		event.change_id = a_command.recordingID;
		event.intent = a_command.intent == WriteIntent::kReset ? 2U : a_command.intent == WriteIntent::kActivate ? 1U :
		                                                                                                           0U;
		event.confirmation_accepted = a_accepted;
		event.confirmation_declined = a_declined;
		for (const auto& mod : a_command.recordingSnapshot->mods) {
			for (const auto& page : mod.pages) {
				for (const auto& control : page.controls) {
					if (control.identity.stableID == a_command.settingID) {
						PublishControl(event, mod, page, control, a_value);
						return;
					}
				}
			}
		}
	}

	void HostEvents::PublishControl(MCMHostEvent a_event, const MCMMod& a_mod, const MCMPage& a_page, const MCMControl& a_control, const MCMValue& a_value)
	{
		a_event.control_type = HostControlType(a_control.type);
		if (!a_event.control_type)
			return;
		a_event.mcm_id = a_mod.interopID.c_str();
		// interopID retains the original name even when the frontend uses an alias.
		a_event.mod_name = HostModName(a_mod.interopID, a_mod.displayName);
		a_event.page_name = a_page.rawName.c_str();
		a_event.page_index = a_page.index;
		a_event.option_index = a_control.identity.optionIndex;
		a_event.label = a_control.rawLabel.c_str();
		a_event.setting_id = a_control.identity.explicitID.c_str();
		a_event.state_name = a_control.identity.stateName.c_str();
		a_event.page_scoped_state = a_mod.pageScopedState ? 1U : 0U;
		a_event.value = HostValue(a_value);
		a_event.value_text = "";
		if (const auto* integer = std::get_if<std::int32_t>(&a_value)) {
			if (a_control.menu && *integer >= 0 && static_cast<std::size_t>(*integer) < a_control.menu->options.size())
				a_event.value_text = a_control.menu->options[static_cast<std::size_t>(*integer)].c_str();
		} else if (const auto* text = std::get_if<std::string>(&a_value)) {
			a_event.value_text = text->c_str();
		}
		Publish(a_event);
	}
}
