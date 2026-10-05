#pragma once

#include "MCMBridge/Core/Model.h"
#include "MCMBridge/Core/Result.h"

#include <functional>

namespace MCMBridge
{
	Result<std::reference_wrapper<const MCMControl>> ValidateWrite(
		const MCMSnapshot&  a_snapshot,
		const WriteCommand& a_command);
	// The caller supplies a freshly rebuilt post-commit snapshot, never cached values.
	Result<MCMValue> ConfirmWrite(const MCMSnapshot& a_snapshot, const WriteCommand& a_command, const MCMValue& a_effectiveValue);
}
