#pragma once

#include "MCMBridge/Core/Interfaces.h"
#include "MCMBridge/Core/OperationContext.h"
#include "MCMBridge/Core/OperationTimer.h"
#include "MCMBridge/Papyrus/IClassicScript.h"

#include <chrono>
#include <memory>

namespace MCMBridge
{
	class ClassicScanOperation final : public std::enable_shared_from_this<ClassicScanOperation>
	{
	public:
		using BusyCheck = std::function<bool()>;
		using Completion = std::function<void(Result<MCMMod>)>;

		ClassicScanOperation(
			MCMDescriptor                   a_descriptor,
			std::shared_ptr<IClassicScript> a_script,
			IClassicMenuOptionResolver&     a_menuResolver,
			BusyCheck                       a_busyCheck,
			Completion                      a_completion,
			IOperationTimer&                a_timer,
			const IOperationClock&          a_clock = SteadyOperationClock::GetSingleton());

		void Start();
		void SetNavigationOnly(bool a_value) { navigationOnly = a_value; }
		void SetKeepOpen(bool a_value) { keepOpen = a_value; }
		void SetAlreadyOpen(bool a_value) { configOpen = a_value; }
		void SetReuseCurrentPage(bool a_value) { reuseCurrentPage = a_value; }
		void SetPageTransform(std::function<MCMPage(MCMPage)> a_transform) { pageTransform = std::move(a_transform); }
		void SetMetadataFilter(std::function<bool(const MCMControl&)> a_filter) { metadataFilter = std::move(a_filter); }
		void SetPageFilter(std::function<bool(const ClassicPageSelection&)> a_filter) { pageFilter = std::move(a_filter); }
		void SetPageObserver(std::function<void(const MCMPage&, const IClassicScript&)> a_observer) { pageObserver = std::move(a_observer); }
		void SetContinuationGuard(std::function<bool()> a_guard) { continuationGuard = std::move(a_guard); }
		void Abandon() { Finish(std::unexpected(BridgeError{ BridgeErrorCode::kTimedOut, "Scan was abandoned" })); }
		void Cancel();
		void YieldToFrontend();

	private:
		void Open();
		void ReadPages();
		void SetPage();
		void ReadPage();
		void ReadNextMetadata();
		void ReadSlider(std::size_t a_controlIndex);
		void ReadMenu(std::size_t a_controlIndex);
		void ReadColor(std::size_t a_controlIndex);
		void ReadInput(std::size_t a_controlIndex);
		void AdvancePage();
		void Close();
		void Fail(BridgeError a_error, bool a_closeConfig);
		void Finish(Result<MCMMod> a_result);
		bool CheckBudget();

		bool Dispatch(
			ClassicCall           a_call,
			std::function<void()> a_next);

		void Continue(std::uint64_t a_token, std::function<void()> a_next);
		void ArmTimeout(std::uint64_t a_token, ClassicMethod a_method);

		std::shared_ptr<IClassicScript>                            script;
		IClassicMenuOptionResolver&                                menuResolver;
		BusyCheck                                                  busyCheck;
		Completion                                                 completion;
		IOperationTimer&                                           timer;
		OperationDeadline                                          deadline{ timer };
		OperationContext                                           operation;
		MCMMod                                                     mod;
		std::vector<ClassicPageSelection>                          pages;
		std::size_t                                                pageIndex{};
		std::size_t                                                metadataIndex{};
		std::optional<BridgeError>                                 pendingError;
		bool                                                       configOpen{};
		bool                                                       closing{};
		bool                                                       finished{};
		bool                                                       navigationOnly{};
		bool                                                       keepOpen{};
		bool                                                       reuseCurrentPage{};
		bool                                                       openedHere{};
		std::function<MCMPage(MCMPage)>                            pageTransform;
		std::function<bool(const MCMControl&)>                     metadataFilter;
		std::function<bool(const ClassicPageSelection&)>           pageFilter;
		std::function<void(const MCMPage&, const IClassicScript&)> pageObserver;
		std::function<bool()>                                      continuationGuard;
	};
}
