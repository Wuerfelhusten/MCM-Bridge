#include "MCMBridge/Papyrus/SkyUIFrontendState.h"

namespace
{
	bool EndsWith(std::string_view a_value, std::string_view a_suffix)
	{
		return a_value.size() >= a_suffix.size() && a_value.ends_with(a_suffix);
	}
}

namespace MCMBridge
{
	SkyUIFrontendState& SkyUIFrontendState::GetSingleton()
	{
		static SkyUIFrontendState singleton;
		return singleton;
	}

	void SkyUIFrontendState::BeginPageCapture()
	{
		const std::scoped_lock lock(mutex);
		pageTitle.clear();
	}

	void SkyUIFrontendState::BeginInfoCapture()
	{
		const std::scoped_lock lock(mutex);
		infoText.clear();
	}

	void SkyUIFrontendState::ObserveString(std::string_view a_target, std::string a_value)
	{
		const std::scoped_lock lock(mutex);
		if (EndsWith(a_target, ".setTitleText")) {
			pageTitle = std::move(a_value);
		} else if (EndsWith(a_target, ".setInfoText")) {
			infoText = std::move(a_value);
		}
	}

	std::string SkyUIFrontendState::PageTitle() const
	{
		const std::scoped_lock lock(mutex);
		return pageTitle;
	}

	std::string SkyUIFrontendState::InfoText() const
	{
		const std::scoped_lock lock(mutex);
		return infoText;
	}
}
