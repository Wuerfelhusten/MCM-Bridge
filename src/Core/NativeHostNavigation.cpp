#include "MCMBridge/Core/NativeHostSession.h"
#include <utility>

namespace MCMBridge
{
	void NativeHostSession::BeginIdentityCapture(std::int32_t a_token)
	{
		const std::scoped_lock lock(mutex);
		if (!Owns(a_token) || building)
			return;
		// A new full pass replaces historical page evidence. Changes to pages already
		// captured in this pass still invalidate it, as do reset/navigation requests.
		identityPages.clear();
		if (hasPage)
			identityPages.emplace(std::pair{ working.page, working.index }, working.buffers);
	}

	std::optional<NativeHostIdentity> NativeHostSession::ReadIdentity(std::int32_t a_token) const
	{
		const std::scoped_lock lock(mutex);
		return Owns(a_token) ? std::optional(NativeHostIdentity{ session, working.modID }) : std::nullopt;
	}
	bool NativeHostSession::RequestClose(std::int32_t a_token, bool a_closeFrontend)
	{
		const std::scoped_lock lock(mutex);
		if (!Owns(a_token))
			return false;
		closeRequest = closeRequest.value_or(false) || a_closeFrontend;
		++identityRevision;
		pageRequest.reset();
		return true;
	}
	std::optional<bool> NativeHostSession::TakeCloseRequest(std::int32_t a_token)
	{
		const std::scoped_lock lock(mutex);
		return Owns(a_token) ? std::exchange(closeRequest, std::nullopt) : std::nullopt;
	}
	bool NativeHostSession::RequestPage(std::int32_t a_token, std::string a_name, std::int32_t a_index)
	{
		const std::scoped_lock lock(mutex);
		if (!Owns(a_token) || a_index < 0 || closeRequest.has_value())
			return false;
		pageRequest = NativeHostPageRequest{ std::move(a_name), a_index };
		++identityRevision;
		return true;
	}

	std::optional<NativeHostPageRequest> NativeHostSession::TakePageRequest(std::int32_t a_token)
	{
		const std::scoped_lock lock(mutex);
		return Owns(a_token) ? std::exchange(pageRequest, std::nullopt) : std::nullopt;
	}
	void NativeHostSession::ClearNavigationOverride(std::int32_t a_token)
	{
		const std::scoped_lock lock(mutex);
		if (Owns(a_token))
			navigationOverride = false;
	}
	bool NativeHostSession::SetNavigation(std::int32_t a_token, std::vector<std::string> a_pages)
	{
		const std::scoped_lock lock(mutex);
		if (!Owns(a_token))
			return false;
		navigationOverride = true;
		if (navigation && *navigation == a_pages)
			return true;
		navigation = std::move(a_pages);
		++working.resetRevision;
		++identityRevision;
		return true;
	}

	std::optional<std::vector<std::string>> NativeHostSession::ReadNavigation(std::int32_t a_token) const
	{
		const std::scoped_lock lock(mutex);
		return Owns(a_token) && navigationOverride ? navigation : std::nullopt;
	}
}
