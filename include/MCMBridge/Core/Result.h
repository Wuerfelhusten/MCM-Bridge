#pragma once

#include <expected>
#include <string>

namespace MCMBridge
{
	enum class BridgeErrorCode
	{
		kUnavailable,
		kInvalidData,
		kNotFound,
		kBusy,
		kTimedOut,
		kStaleSnapshot,
		kUnsupported,
		kDispatchFailed,
		kIoError,
		kAmbiguous,
		kPageRebuilt
	};

	struct BridgeError
	{
		BridgeErrorCode code{ BridgeErrorCode::kInvalidData };
		std::string     message;
	};

	template <class T>
	using Result = std::expected<T, BridgeError>;
}
