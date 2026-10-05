#include "MCMBridge/Core/NativeHostSession.h"

namespace MCMBridge
{
	bool NativeHostSession::SetCustomContent(std::int32_t a_token, std::string a_source, float a_x, float a_y)
	{
		const std::scoped_lock lock(mutex);
		if (!Owns(a_token) || !hasPage)
			return false;
		working.customSource = std::move(a_source);
		working.customX = a_x;
		working.customY = a_y;
		return true;
	}

	bool NativeHostSession::IsActive(std::int32_t a_token) const
	{
		const std::scoped_lock lock(mutex);
		return Owns(a_token);
	}

	bool NativeHostSession::SetPresentation(std::int32_t a_token, std::int32_t a_kind, std::string a_text)
	{
		const std::scoped_lock lock(mutex);
		if (!Owns(a_token) || !hasPage)
			return false;
		switch (a_kind) {
		case 0:
			working.title = std::move(a_text);
			break;
		case 1:
			working.info = std::move(a_text);
			break;
		case 2:
			working.customSource = std::move(a_text);
			break;
		case 3:
			working.customSource.clear();
			working.customX = 0;
			working.customY = 0;
			break;
		case 4:
			++working.resetRevision;
			++identityRevision;
			break;
		default:
			return false;
		}
		return true;
	}
}
