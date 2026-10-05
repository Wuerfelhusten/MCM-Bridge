#pragma once

#include "MCMBridge/Core/Interfaces.h"
#include "MCMBridge/Core/OperationContext.h"
#include "MCMBridge/Core/OperationTimer.h"
#include "MCMBridge/Papyrus/IClassicScript.h"

#include <chrono>
#include <memory>

namespace MCMBridge
{
	enum class ClassicWriteMode
	{
		kStandalone,
		kHosted
	};

	class ClassicWriteOperation final : public std::enable_shared_from_this<ClassicWriteOperation>
	{
	public:
		using BusyCheck = std::function<bool()>;
		using Completion = std::function<void(Result<MCMValue>)>;

		ClassicWriteOperation(
			std::shared_ptr<IClassicScript> a_script,
			MCMControl                      a_control,
			WriteCommand                    a_command,
			BusyCheck                       a_busyCheck,
			Completion                      a_completion,
			IOperationTimer&                a_timer,
			ClassicWriteMode                a_mode = ClassicWriteMode::kStandalone,
			const IOperationClock&          a_clock = SteadyOperationClock::GetSingleton());

		void                                Start();
		void                                Cancel();
		void                                YieldToFrontend();
		void                                SetMenuResolver(IClassicMenuOptionResolver& a_resolver) { menuResolver = &a_resolver; }
		std::optional<ClassicPageSelection> PageRedirect() const { return pageRedirect; }
		void                                Abandon() { Finish(std::unexpected(BridgeError{ BridgeErrorCode::kTimedOut, "Write was abandoned" })); }
		void                                SetPagePrepared(bool a_value) { pagePrepared = a_value; }

	private:
		void                    Open();
		void                    SetPage(bool a_confirming);
		void                    ValidateAndApply();
		void                    ApplyText();
		void                    ApplyToggle(const MCMValue& a_current);
		void                    ApplyKeymap();
		void                    ApplyReset();
		void                    RequestSlider();
		void                    RequestMenu(bool a_confirming);
		void                    RequestColor(bool a_confirming);
		void                    RequestInput();
		void                    DispatchDialog(ClassicCall a_call, std::function<void()> a_next);
		std::optional<MCMValue> ReadCurrentValue() const;
		void                    Confirm();
		void                    Close(Result<MCMValue> a_result);
		void                    Finish(Result<MCMValue> a_result);
		bool                    CheckBudget();
		bool                    Dispatch(ClassicCall a_call, std::function<void()> a_next);
		void                    Continue(std::uint64_t a_token, std::function<void()> a_next);
		void                    ArmTimeout(std::uint64_t a_token);

		std::shared_ptr<IClassicScript>     script;
		MCMControl                          control;
		WriteCommand                        command;
		BusyCheck                           busyCheck;
		Completion                          completion;
		IOperationTimer&                    timer;
		OperationDeadline                   deadline{ timer };
		bool                                pagePrepared{ true };
		std::uint64_t                       resetBeforeCallback{};
		bool                                callbackResetPage{};
		std::optional<ClassicPageSelection> pageRedirect;
		OperationContext                    operation;
		ClassicWriteMode                    mode;
		std::optional<Result<MCMValue>>     closeResult;
		bool                                configOpen{};
		bool                                closing{};
		bool                                finished{};
		IClassicMenuOptionResolver*         menuResolver{};
		bool                                menuCaptureActive{};
	};
}
