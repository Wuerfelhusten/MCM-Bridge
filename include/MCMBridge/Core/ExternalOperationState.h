#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace MCMBridge
{
	enum class ExternalAdmission
	{
		kReady,
		kBusy,
		kInvalid
	};

	// The controller retains ownership until outstanding VM work and cleanup finish.
	class ExternalOperationState
	{
	public:
		ExternalAdmission Begin(std::string_view a_owner, bool a_busy, bool a_writesPending, std::uint64_t& a_token);
		bool              End(std::uint64_t a_token);
		bool              Cancel(std::string_view a_owner);
		void              Reset();
		bool              IsWaitingFor(std::string_view a_owner) const { return token == 0 && owner == a_owner; }
		bool              BlocksNormalWork() const { return !owner.empty(); }

	private:
		std::string   owner;
		std::uint64_t token{};
		std::uint64_t nextToken{ 1 };
	};
}
