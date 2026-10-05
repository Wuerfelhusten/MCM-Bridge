#include "MCMBridge/Core/HostPageCapture.h"

namespace MCMBridge
{
	HostCaptureToken HostPageCapture::Begin(std::uint64_t a_session, std::string a_page, std::int32_t a_index)
	{
		Cancel();
		token.session = a_session;
		active = a_session != 0;
		page = std::move(a_page);
		index = a_index;
		return token;
	}

	void HostPageCapture::Cancel()
	{
		++token.operation;
		active = false;
		malformed = false;
		flags.reset();
		labels.reset();
		strings.reset();
		numbers.reset();
		completed.reset();
	}

	bool HostPageCapture::Observe(HostCaptureToken a_token, std::string_view a_method, const HostBufferPayload& a_payload)
	{
		if (!active || a_token != token)
			return false;
		const auto store = [&]<class T>(std::optional<std::vector<T>>& a_buffer) {
			if (const auto* values = std::get_if<std::vector<T>>(&a_payload)) {
				a_buffer = *values;
			} else {
				malformed = true;
			}
		};
		if (a_method == "setOptionFlagsBuffer") {
			store(flags);
		} else if (a_method == "setOptionTextBuffer") {
			store(labels);
		} else if (a_method == "setOptionStrValueBuffer") {
			store(strings);
		} else if (a_method == "setOptionNumValueBuffer") {
			store(numbers);
		} else if (a_method == "flushOptionBuffers") {
			const auto* count = std::get_if<std::int32_t>(&a_payload);
			if (!count || *count < 0 || !flags || !labels || !strings || !numbers) {
				malformed = true;
			} else {
				const auto size = static_cast<std::size_t>(*count);
				if (size > flags->size() || size > labels->size() || size > strings->size() || size > numbers->size()) {
					malformed = true;
				} else if (!malformed) {
					auto result = std::make_shared<HostCapturedPage>();
					result->page = page;
					result->index = index;
					result->buffers.optionFlags = std::move(*flags);
					result->buffers.labels = std::move(*labels);
					result->buffers.stringValues = std::move(*strings);
					result->buffers.numericValues = std::move(*numbers);
					result->buffers.optionFlags.resize(size);
					result->buffers.labels.resize(size);
					result->buffers.stringValues.resize(size);
					result->buffers.numericValues.resize(size);
					// State mappings are not part of the UI buffer protocol. The adapter
					// must reconcile them before treating these controls as write targets.
					completed = std::move(result);
				}
			}
			flags.reset();
			labels.reset();
			strings.reset();
			numbers.reset();
		} else {
			return false;
		}
		return true;
	}

	std::shared_ptr<const HostCapturedPage> HostPageCapture::Complete(HostCaptureToken a_token)
	{
		if (!active || a_token != token)
			return {};
		// Partial writes after a flush cannot be published as a complete final page.
		auto result = !malformed && !flags && !labels && !strings && !numbers ? completed : nullptr;
		Cancel();
		return result;
	}
}
