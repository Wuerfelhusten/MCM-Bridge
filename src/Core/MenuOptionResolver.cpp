#include "MCMBridge/Core/MenuOptionResolver.h"

namespace MCMBridge
{
	void UnavailableMenuOptionResolver::BeginCapture(const SettingIdentity&)
	{}

	void UnavailableMenuOptionResolver::CancelCapture()
	{}

	Result<MenuMetadata> UnavailableMenuOptionResolver::Resolve(const SettingIdentity&)
	{
		return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "Classic menu options were not captured" });
	}

	void ScopedMenuOptionResolver::BeginCapture(const SettingIdentity& a_identity)
	{
		const std::scoped_lock lock(mutex);
		activeIdentity = a_identity;
		captured.reset();
	}

	void ScopedMenuOptionResolver::CancelCapture()
	{
		const std::scoped_lock lock(mutex);
		activeIdentity.reset();
		captured.reset();
	}

	Result<MenuMetadata> ScopedMenuOptionResolver::Resolve(const SettingIdentity& a_identity)
	{
		const std::scoped_lock lock(mutex);
		if (!activeIdentity || activeIdentity->stableID != a_identity.stableID || !captured) {
			return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "No scoped menu option capture is available" });
		}
		auto result = *captured;
		activeIdentity.reset();
		captured.reset();
		return result;
	}

	void ScopedMenuOptionResolver::ObserveInvokeStringArray(
		std::string_view         a_menuName,
		std::string_view         a_target,
		std::vector<std::string> a_options)
	{
		const std::scoped_lock lock(mutex);
		if (!activeIdentity || a_menuName != "Journal Menu" ||
			a_target != "_root.ConfigPanelFader.configPanel.setMenuDialogOptions" || a_options.empty()) {
			return;
		}
		captured = MenuMetadata{
			.options = std::move(a_options),
			.availability = MetadataAvailability::kAvailable
		};
	}
}
