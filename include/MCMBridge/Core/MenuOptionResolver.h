#pragma once

#include "MCMBridge/Core/Interfaces.h"

#include <mutex>

namespace MCMBridge
{
	class UnavailableMenuOptionResolver final : public IClassicMenuOptionResolver
	{
	public:
		void                 BeginCapture(const SettingIdentity& a_identity) override;
		void                 CancelCapture() override;
		Result<MenuMetadata> Resolve(const SettingIdentity& a_identity) override;
	};

	class ScopedMenuOptionResolver final : public IClassicMenuOptionResolver
	{
	public:
		void                 BeginCapture(const SettingIdentity& a_identity) override;
		void                 CancelCapture() override;
		Result<MenuMetadata> Resolve(const SettingIdentity& a_identity) override;

		void ObserveInvokeStringArray(
			std::string_view         a_menuName,
			std::string_view         a_target,
			std::vector<std::string> a_options);

	private:
		std::mutex                     mutex;
		std::optional<SettingIdentity> activeIdentity;
		std::optional<MenuMetadata>    captured;
	};
}
