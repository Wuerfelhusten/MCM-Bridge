#pragma once

#include "MCMBridge/Core/Model.h"

namespace MCMBridge
{
	std::shared_ptr<WriteTiming> MakeWriteTiming(const WriteCommand& a_command);
}
