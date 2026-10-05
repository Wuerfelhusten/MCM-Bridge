#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>

namespace MCMBridge
{
	class SkyUIFrontendState
	{
	public:
		static SkyUIFrontendState& GetSingleton();

		void          BeginPageCapture();
		void          BeginInfoCapture();
		void          ObserveString(std::string_view a_target, std::string a_value);
		std::string   PageTitle() const;
		std::string   InfoText() const;
		void          InvalidatePage() { pageRevision.fetch_add(1); }
		std::uint64_t PageRevision() const { return pageRevision.load(); }

	private:
		mutable std::mutex         mutex;
		std::string                pageTitle;
		std::string                infoText;
		std::atomic<std::uint64_t> pageRevision{};
	};
}
