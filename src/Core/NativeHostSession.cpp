#include "MCMBridge/Core/NativeHostSession.h"

#include "MCMBridge/Core/HostAudit.h"
#include "MCMBridge/Core/NativePreflightPolicy.h"

#include <algorithm>
#include <format>
#include <limits>

namespace
{
	constexpr std::size_t optionCount = 128;
	bool                  ValidFlags(std::int32_t a_flags) { return a_flags >= 0 && a_flags <= 0x7FFFFF; }

	std::string IdentityChange(const MCMBridge::ClassicPageBuffers& a_before, const MCMBridge::ClassicPageBuffers& a_after, bool a_building)
	{
		for (std::size_t slot = 0; slot < optionCount; ++slot) {
			if (a_before.optionFlags[slot] != a_after.optionFlags[slot])
				return std::format("flags slot={} before={} after={}", slot, a_before.optionFlags[slot], a_after.optionFlags[slot]);
			if (a_before.stateNames[slot] != a_after.stateNames[slot])
				return std::format("state slot={} before={} after={}", slot, a_before.stateNames[slot], a_after.stateNames[slot]);
			// Header captions are presentation, not writable target identities. Their
			// type, position and state remain covered by the checks above.
			if ((a_after.optionFlags[slot] & 0xFF) != 1 && a_before.labels[slot] != a_after.labels[slot])
				return std::format("label slot={} before={} after={}", slot, a_before.labels[slot], a_after.labels[slot]);
			if (!a_building && (a_before.numericValues[slot] != a_after.numericValues[slot] || a_before.stringValues[slot] != a_after.stringValues[slot]))
				return std::format("external_value slot={}", slot);
		}
		return {};
	}
}

namespace MCMBridge
{
	std::uint64_t NativeHostSession::Session() const
	{
		const std::scoped_lock lock(mutex);
		return session;
	}

	std::uint64_t NativeHostSession::IdentityRevision() const
	{
		const std::scoped_lock lock(mutex);
		return identityRevision;
	}

	void NativeHostSession::Reset(std::uint64_t a_session)
	{
		const std::scoped_lock lock(mutex);
		session = a_session;
		++identityRevision;
		token = 0;
		externalOwner = 0;
		messageRequest = 0;
		messageWait = {};
		messageResult.reset();
		dialog.reset();
		completedDialog.reset();
		building = hasPage = false;
		working = {};
		identityPages.clear();
		navigation.reset();
		published.reset();
		pageRequest.reset();
		closeRequest.reset();
	}

	Result<std::int32_t> NativeHostSession::Open(std::uint64_t a_session, std::string a_modID, std::uint64_t a_externalOwner)
	{
		const std::scoped_lock lock(mutex);
		if (!session || session != a_session)
			return std::unexpected(BridgeError{ BridgeErrorCode::kStaleSnapshot, "Native host session changed" });
		if (token)
			return std::unexpected(BridgeError{ BridgeErrorCode::kBusy, "Native host already has an execution owner" });
		if (a_modID.empty() || nextToken == std::numeric_limits<std::int32_t>::max())
			return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "Native host identity missing or tokens exhausted" });
		working = { .session = session, .modID = std::move(a_modID) };
		identityPages.clear();
		navigation.reset();
		pageRequest.reset();
		closeRequest.reset();
		building = hasPage = false;
		token = ++nextToken;
		messageWait = {};
		++identityRevision;
		externalOwner = a_externalOwner;
		return token;
	}

	bool NativeHostSession::Owns(std::int32_t a_token) const { return a_token > 0 && token == a_token; }

	bool NativeHostSession::Close(std::int32_t a_token)
	{
		const std::scoped_lock lock(mutex);
		if (!Owns(a_token))
			return false;
		++identityRevision;
		token = 0;
		externalOwner = 0;
		messageRequest = 0;
		messageResult.reset();
		dialog.reset();
		completedDialog.reset();
		building = hasPage = false;
		working = {};
		navigation.reset();
		pageRequest.reset();
		closeRequest.reset();
		return true;
	}

	bool NativeHostSession::BeginPage(std::int32_t a_token, std::string a_page, std::int32_t a_index)
	{
		const std::scoped_lock lock(mutex);
		if (!Owns(a_token) || a_index < -1 || a_index >= std::numeric_limits<std::int32_t>::max() / 256)
			return false;
		ClassicPageBuffers buffers;
		buffers.optionFlags.resize(optionCount);
		buffers.labels.resize(optionCount);
		buffers.stringValues.resize(optionCount);
		buffers.numericValues.resize(optionCount);
		buffers.stateNames.resize(optionCount);
		working.page = std::move(a_page);
		working.index = a_index;
		working.buffers = std::move(buffers);
		dialog.reset();
		completedDialog.reset();
		working.title = working.page;
		working.info.clear();
		working.customSource.clear();
		working.customX = 0;
		working.customY = 0;
		cursor = 0;
		optionCursor = -1;
		fillMode = 1;
		building = hasPage = true;
		return true;
	}

	bool NativeHostSession::SetCursor(std::int32_t a_token, std::int32_t a_position, std::int32_t a_fillMode)
	{
		const std::scoped_lock lock(mutex);
		if (!Owns(a_token) || !building || a_position < -1 || a_position >= static_cast<std::int32_t>(optionCount) || (a_fillMode != 1 && a_fillMode != 2))
			return false;
		cursor = a_position;
		fillMode = a_fillMode;
		return true;
	}

	std::int32_t NativeHostSession::AddOption(std::int32_t a_token, std::int32_t a_type, std::string a_label,
		std::string a_text, float a_value, std::int32_t a_flags, std::string a_state)
	{
		const std::scoped_lock lock(mutex);
		if (!Owns(a_token) || !building || cursor < 0 || a_type < 0 || a_type > 8 || !ValidFlags(a_flags))
			return -1;
		auto& buffers = working.buffers;
		if (!a_state.empty() && std::ranges::any_of(buffers.stateNames, [&](const auto& a_name) { return EqualPapyrusTypeName(a_name, a_state); }))
			return -1;
		const auto index = static_cast<std::size_t>(cursor);
		if (!a_state.empty() && !buffers.stateNames[index].empty())
			return -1;
		buffers.optionFlags[index] = a_type + a_flags * 256;
		buffers.labels[index] = std::move(a_label);
		buffers.stringValues[index] = std::move(a_text);
		buffers.numericValues[index] = a_value;
		if (!a_state.empty())
			buffers.stateNames[index] = std::move(a_state);
		const auto id = cursor + (working.index + 1) * 256;
		cursor += fillMode;
		if (cursor >= static_cast<std::int32_t>(optionCount))
			cursor = -1;
		return id;
	}

	std::optional<std::size_t> NativeHostSession::Resolve(std::int32_t a_optionID) const
	{
		if (!hasPage || a_optionID < 0 || a_optionID / 256 != working.index + 1)
			return std::nullopt;
		const auto index = static_cast<std::size_t>(a_optionID % 256);
		return index < optionCount ? std::optional(index) : std::nullopt;
	}

	bool NativeHostSession::SetValue(std::int32_t a_token, std::int32_t a_optionID, std::string a_text, float a_value)
	{
		const std::scoped_lock lock(mutex);
		const auto             index = Resolve(a_optionID);
		if (!Owns(a_token) || building || !index)
			return false;
		working.buffers.stringValues[*index] = std::move(a_text);
		working.buffers.numericValues[*index] = a_value;
		return true;
	}

	bool NativeHostSession::SetFlags(std::int32_t a_token, std::int32_t a_optionID, std::int32_t a_flags)
	{
		const std::scoped_lock lock(mutex);
		const auto             index = Resolve(a_optionID);
		if (!Owns(a_token) || !index || !ValidFlags(a_flags))
			return false;
		auto& flags = working.buffers.optionFlags[*index];
		flags = (flags & 0xFF) + a_flags * 256;
		return true;
	}

	bool NativeHostSession::ImportBuffers(std::int32_t a_token, ClassicPageBuffers a_buffers)
	{
		const std::scoped_lock lock(mutex);
		if (!Owns(a_token) || !hasPage || a_buffers.optionFlags.size() != optionCount || a_buffers.labels.size() != optionCount ||
			a_buffers.stringValues.size() != optionCount || a_buffers.numericValues.size() != optionCount || a_buffers.stateNames.size() != optionCount)
			return false;
		working.buffers = std::move(a_buffers);
		return true;
	}

	bool NativeHostSession::Publish(std::int32_t a_token)
	{
		std::unique_lock lock(mutex);
		if (!Owns(a_token) || !hasPage)
			return false;
		const auto  key = std::pair{ working.page, working.index };
		const auto  previous = identityPages.find(key);
		std::string audit;
		if (previous != identityPages.end()) {
			const auto reason = IdentityChange(previous->second, working.buffers, building);
			if (!reason.empty()) {
				++identityRevision;
				audit = std::format("mod={} page={} index={} revision={} building={} reason={}", working.modID, working.page, working.index, identityRevision, building, reason);
			}
		}
		identityPages.insert_or_assign(key, working.buffers);
		const bool samePage = published && published->session == session && published->modID == working.modID && published->page == working.page && published->index == working.index;
		const bool sameStructure = samePage && published->buffers.labels == working.buffers.labels && published->buffers.stateNames == working.buffers.stateNames &&
		                           std::ranges::equal(published->buffers.optionFlags, working.buffers.optionFlags, [](auto a_left, auto a_right) { return (a_left & 0xFF) == (a_right & 0xFF); });
		const bool sameValues = sameStructure && published->buffers.optionFlags == working.buffers.optionFlags && published->buffers.stringValues == working.buffers.stringValues && published->buffers.numericValues == working.buffers.numericValues && published->title == working.title && published->info == working.info && published->customSource == working.customSource && published->customX == working.customX && published->customY == working.customY && published->resetRevision == working.resetRevision;
		working.structureRevision = (published ? published->structureRevision : 0) + (sameStructure ? 0 : 1);
		working.valueRevision = (published ? published->valueRevision : 0) + (sameValues ? 0 : 1);
		if (!sameValues)
			published = std::make_shared<const NativeHostPage>(working);
		building = false;
		lock.unlock();
		if (!audit.empty())
			HostAudit("Native identity changed: {}", audit);
		return true;
	}

	std::shared_ptr<const NativeHostPage> NativeHostSession::Read() const
	{
		const std::scoped_lock lock(mutex);
		return published;
	}
}
