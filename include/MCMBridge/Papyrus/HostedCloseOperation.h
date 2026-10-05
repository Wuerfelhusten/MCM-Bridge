#pragma once

#include "MCMBridge/Core/OperationContext.h"
#include "MCMBridge/Core/OperationTimer.h"
#include "MCMBridge/Core/Result.h"
#include "MCMBridge/Papyrus/IClassicScript.h"

#include <chrono>
#include <memory>

namespace MCMBridge
{
	class HostedCloseOperation final : public std::enable_shared_from_this<HostedCloseOperation>
	{
	public:
		using Completion = std::function<void(Result<void>)>;

		HostedCloseOperation(
			std::shared_ptr<IClassicScript> a_script,
			Completion                      a_completion,
			IOperationTimer&                a_timer,
			const IOperationClock&          a_clock = SteadyOperationClock::GetSingleton());

		void Start();
		void Cancel();

	private:
		void Finish(Result<void> a_result);
		void ArmTimeout(std::uint64_t a_token);

		std::shared_ptr<IClassicScript> script;
		Completion                      completion;
		IOperationTimer&                timer;
		OperationDeadline               deadline{ timer };
		OperationContext                operation;
		bool                            finished{};
	};
}
