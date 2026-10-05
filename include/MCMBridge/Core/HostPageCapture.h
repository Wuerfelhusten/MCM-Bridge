#pragma once

#include "MCMBridge/Core/ClassicParser.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace MCMBridge
{
	struct HostCaptureToken
	{
		std::uint64_t session{};
		std::uint64_t operation{};
		bool          operator==(const HostCaptureToken&) const = default;
	};

	struct HostCapturedPage
	{
		std::string        page;
		std::int32_t       index{};
		ClassicPageBuffers buffers;
	};

	using HostBufferPayload = std::variant<std::vector<std::int32_t>, std::vector<float>, std::vector<std::string>, std::int32_t, float, bool, std::string>;

	enum class HostProtocolCallKind
	{
		kInvoke,
		kSet
	};

	// The session owner serializes access. The adapter must authenticate the caller
	// before supplying its token; matching a UI target name alone is insufficient.
	class HostPageCapture
	{
	public:
		HostCaptureToken                        Begin(std::uint64_t a_session, std::string a_page, std::int32_t a_index);
		bool                                    Observe(HostCaptureToken a_token, std::string_view a_method, const HostBufferPayload& a_payload);
		std::shared_ptr<const HostCapturedPage> Complete(HostCaptureToken a_token);
		void                                    Cancel();

	private:
		HostCaptureToken                         token;
		bool                                     active{};
		bool                                     malformed{};
		std::string                              page;
		std::int32_t                             index{};
		std::optional<std::vector<std::int32_t>> flags;
		std::optional<std::vector<std::string>>  labels;
		std::optional<std::vector<std::string>>  strings;
		std::optional<std::vector<float>>        numbers;
		std::shared_ptr<const HostCapturedPage>  completed;
	};
}
