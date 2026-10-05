#pragma once

#include "MCMBridge/Core/OperationContext.h"
#include "MCMBridge/Core/OperationTimer.h"
#include "MCMBridge/Papyrus/IClassicScript.h"

#include <memory>

namespace MCMBridge
{
	class ClassicHelpOperation final : public std::enable_shared_from_this<ClassicHelpOperation>
	{
	public:
		using BusyCheck = std::function<bool()>;
		using Completion = std::function<void(Result<std::string>)>;

		ClassicHelpOperation(
			std::shared_ptr<IClassicScript> a_script,
			SettingIdentity                 a_identity,
			BusyCheck                       a_busyCheck,
			Completion                      a_completion,
			IOperationTimer&                a_timer,
			const IOperationClock&          a_clock = SteadyOperationClock::GetSingleton());

		void Start();
		void Cancel();

	private:
		void Finish(Result<std::string> a_result);
		void ArmTimeout(std::uint64_t a_token);

		std::shared_ptr<IClassicScript> script;
		SettingIdentity                 identity;
		BusyCheck                       busyCheck;
		Completion                      completion;
		IOperationTimer&                timer;
		OperationDeadline               deadline{ timer };
		OperationContext                operation;
		bool                            finished{};
	};
}
