#include "MCMBridge/Core/ExternalOperationState.h"

namespace MCMBridge
{
	ExternalAdmission ExternalOperationState::Begin(std::string_view a_owner, bool a_busy, bool a_writesPending, std::uint64_t& a_token)
	{
		a_token = 0;
		if (a_owner.empty() || !nextToken)
			return ExternalAdmission::kInvalid;
		if ((owner.empty() && a_writesPending) || token || (!owner.empty() && owner != a_owner))
			return ExternalAdmission::kBusy;
		owner = a_owner;
		if (a_busy)
			return ExternalAdmission::kBusy;
		a_token = token = nextToken++;
		return ExternalAdmission::kReady;
	}

	bool ExternalOperationState::End(std::uint64_t a_token)
	{
		if (!a_token || token != a_token)
			return false;
		Reset();
		return true;
	}

	bool ExternalOperationState::Cancel(std::string_view a_owner)
	{
		if (owner.empty() || owner != a_owner)
			return false;
		if (!token)
			Reset();
		return true;
	}

	void ExternalOperationState::Reset()
	{
		token = 0;
		owner.clear();
	}
}
