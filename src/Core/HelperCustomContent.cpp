#include "MCMBridge/Core/HelperCustomContent.h"

#include "MCMBridge/Core/HelperMenuCapture.h"

#include <cmath>
#include <cstring>

namespace MCMBridge
{
	Result<CustomContentMetadata> CopyHelperCustomContent(std::span<const std::byte> a_record)
	{
		if (a_record.size() != 48)
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Helper custom content layout is incomplete" });
		auto strings = CopyHelperMenuStrings(a_record.subspan(8, 32));
		if (!strings)
			return std::unexpected(strings.error());
		CustomContentMetadata content;
		content.source = std::move(strings->front());
		std::memcpy(&content.x, a_record.data() + 40, sizeof(float));
		std::memcpy(&content.y, a_record.data() + 44, sizeof(float));
		if (!std::isfinite(content.x) || !std::isfinite(content.y) || content.source.find('\0') != std::string::npos)
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Helper custom content parameters are invalid" });
		return content;
	}
}
