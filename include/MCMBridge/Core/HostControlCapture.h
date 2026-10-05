#pragma once

#include "MCMBridge/Core/HostPageCapture.h"

namespace MCMBridge
{
	struct HostControlChange
	{
		std::int32_t                index{};
		std::optional<std::int32_t> flags;
		std::optional<float>        number;
		std::optional<std::string>  text;
	};

	struct HostControlChanges
	{
		std::vector<HostControlChange>           controls;
		std::optional<std::vector<std::string>>  navigation;
		std::optional<std::vector<std::string>>  menuOptions;
		std::optional<std::vector<float>>        sliderParameters;
		std::optional<std::vector<std::int32_t>> menuParameters;
		std::optional<std::vector<std::int32_t>> colorParameters;
		std::optional<std::string>               inputText;
		std::optional<std::string>               title;
		std::optional<std::string>               info;
		bool                                     redrawRequested{};
		bool                                     resetRequested{};
		bool                                     malformed{};
	};

	// One instance per authenticated VM call. Cursor state never crosses calls.
	class HostControlCapture
	{
	public:
		bool               Observe(HostProtocolCallKind a_kind, std::string_view a_target, const HostBufferPayload& a_payload);
		HostControlChanges Complete();

	private:
		std::optional<std::int32_t> cursor;
		HostControlChanges          changes;
	};
}
