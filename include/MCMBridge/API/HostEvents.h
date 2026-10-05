#pragma once

#include "MCMBridge/API/MCMBridgeHost.h"
#include "MCMBridge/Core/Model.h"

#include <map>
#include <mutex>

namespace MCMBridge
{
	class HostEvents
	{
	public:
		static HostEvents& GetSingleton();
		MCMHostResult      Subscribe(MCMHostObserver a_observer, void* a_user, std::uint64_t& a_subscription);
		MCMHostResult      Unsubscribe(std::uint64_t a_subscription);
		void               Session(std::uint32_t a_type, std::uint64_t a_session);
		void               Changed(std::uint64_t a_session, const MCMMod& a_mod, const MCMPage& a_page, const MCMControl& a_control, const MCMValue& a_value);
		void               Write(std::uint32_t a_type, std::uint64_t a_session, const WriteCommand& a_command, const MCMValue& a_value, bool a_accepted = false, bool a_declined = false);

	private:
		struct Observer
		{
			MCMHostObserver callback;
			void*           user;
		};
		void                              Publish(const MCMHostEvent& a_event);
		void                              PublishControl(MCMHostEvent a_event, const MCMMod& a_mod, const MCMPage& a_page, const MCMControl& a_control, const MCMValue& a_value);
		std::recursive_mutex              mutex;
		std::map<std::uint64_t, Observer> observers;
		std::uint64_t                     nextID{ 1 };
	};
}
