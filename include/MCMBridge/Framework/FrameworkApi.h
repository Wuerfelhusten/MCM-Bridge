#pragma once

#include "MCMBridge/Core/FrontendSelection.h"
#include "MCMBridge/Core/Model.h"

#include <atomic>
#include <mutex>
#include <span>

namespace MCMBridge
{
	class FrameworkApi
	{
	public:
		static FrameworkApi& GetSingleton();

		bool     BindAndRegister();
		void     SynchronizeMCMs(std::span<const MCMMod> a_mods);
		bool     IsAvailable() const;
		float    Version() const;
		Frontend Active() const;
		bool     HasMenuFramework() const;
		bool     HasFlick() const;
		bool     CanRender(Frontend a_frontend) const;
		bool     CanRenderAuxiliary(Frontend a_frontend) const;
		void     RequestPreferenceSwitch();
		void     InvalidateHandoff();
		void     SetOpen(bool a_open);
		bool     IsOpen() const;
		// Serializes our callbacks across both renderers and preference handoff.
		static std::recursive_mutex& RenderMutex();

	private:
		std::atomic_bool      registered{};
		float                 version{};
		bool                  menuFramework{};
		bool                  flick{};
		std::atomic<Frontend> active{ Frontend::kNone };
		std::atomic_bool      switching{};
		bool                  switchQueued{};
		std::uint64_t         switchEpoch{};
		void                  DrivePreferenceSwitch();
		void                  RegisterMenuFramework();
	};
}
