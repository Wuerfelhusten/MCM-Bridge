#pragma once

#include "MCMBridge/API/MCMBridgeHost.h"
#include "MCMBridge/Papyrus/IClassicScript.h"

namespace MCMBridge
{
	Result<ClassicCall> DecodeHostCall(const MCMHostCall& a_call);
}
