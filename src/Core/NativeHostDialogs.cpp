#include "MCMBridge/Core/NativeHostSession.h"

#include <limits>

namespace MCMBridge
{
	std::int32_t NativeHostSession::TokenForOwner(std::uint64_t a_owner) const
	{
		const std::scoped_lock lock(mutex);
		return a_owner && a_owner == externalOwner ? token : 0;
	}

	std::int32_t NativeHostSession::ActiveDialogRequest(std::int32_t a_token, std::int32_t a_type) const
	{
		const std::scoped_lock lock(mutex);
		return Owns(a_token) && dialog && dialog->type == a_type ? dialog->request : 0;
	}

	std::int32_t NativeHostSession::BeginDialog(std::int32_t a_token, std::int32_t a_optionID)
	{
		const std::scoped_lock lock(mutex);
		const auto             index = Resolve(a_optionID);
		if (!Owns(a_token) || building || !index || dialog || nextDialog == std::numeric_limits<std::int32_t>::max())
			return 0;
		const auto type = working.buffers.optionFlags[*index] & 0xFF;
		if (type != 4 && type != 5 && type != 6 && type != 8)
			return 0;
		dialog = NativeHostDialog{ .request = ++nextDialog, .optionID = a_optionID, .type = type };
		completedDialog.reset();
		return dialog->request;
	}

	std::optional<NativeHostDialog> NativeHostSession::FinishDialog(std::int32_t a_token, std::int32_t a_request)
	{
		const std::scoped_lock lock(mutex);
		if (!Owns(a_token) || !dialog || dialog->request != a_request)
			return std::nullopt;
		completedDialog = *dialog;
		auto result = std::move(dialog);
		dialog.reset();
		return result;
	}

	bool NativeHostSession::CancelDialog(std::int32_t a_token, std::int32_t a_request)
	{
		const std::scoped_lock lock(mutex);
		if (!Owns(a_token) || !dialog || dialog->request != a_request)
			return false;
		dialog.reset();
		return true;
	}

	std::optional<NativeHostDialog> NativeHostSession::ReadDialog(std::int32_t a_token) const
	{
		const std::scoped_lock lock(mutex);
		return Owns(a_token) ? completedDialog : std::nullopt;
	}

	bool NativeHostSession::SetSliderParameter(std::int32_t a_request, std::int32_t a_index, float a_value)
	{
		const std::scoped_lock lock(mutex);
		if (!dialog || dialog->request != a_request || dialog->type != 4 || a_index < 0 || a_index >= 5)
			return false;
		// Preserve callback data exactly. Validation and normalization belong to the write path.
		dialog->slider[static_cast<std::size_t>(a_index)] = a_value;
		return true;
	}

	bool NativeHostSession::SetDialogIndex(std::int32_t a_request, std::int32_t a_type, std::int32_t a_index, std::int32_t a_value)
	{
		const std::scoped_lock lock(mutex);
		if (!dialog || dialog->request != a_request || dialog->type != a_type || (a_type != 5 && a_type != 6) || a_index < 0 || a_index >= 2)
			return false;
		auto& parameters = a_type == 5 ? dialog->menu : dialog->color;
		parameters[static_cast<std::size_t>(a_index)] = a_value;
		return true;
	}

	bool NativeHostSession::SetDialogOptions(std::int32_t a_request, std::vector<std::string> a_options)
	{
		const std::scoped_lock lock(mutex);
		if (!dialog || dialog->request != a_request || dialog->type != 5)
			return false;
		dialog->options = std::move(a_options);
		return true;
	}

	bool NativeHostSession::SetDialogInput(std::int32_t a_request, std::string a_text)
	{
		const std::scoped_lock lock(mutex);
		if (!dialog || dialog->request != a_request || dialog->type != 8)
			return false;
		dialog->input = std::move(a_text);
		return true;
	}
}
