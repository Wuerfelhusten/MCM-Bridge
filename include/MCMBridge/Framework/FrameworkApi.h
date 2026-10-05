#pragma once

#include "MCMBridge/Core/Model.h"

#include <span>

namespace MCMBridge
{
	class FrameworkApi
	{
	public:
		static FrameworkApi& GetSingleton();

		bool  BindAndRegister();
		void  SynchronizeMCMs(std::span<const MCMMod> a_mods);
		bool  IsAvailable() const;
		float Version() const;

	private:
		bool  registered{};
		float version{};
	};
}
