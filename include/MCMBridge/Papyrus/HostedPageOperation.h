#pragma once

#include "MCMBridge/Core/Interfaces.h"
#include "MCMBridge/Core/OperationContext.h"
#include "MCMBridge/Core/OperationTimer.h"
#include "MCMBridge/Papyrus/IClassicScript.h"

#include <chrono>
#include <memory>

namespace MCMBridge
{
	enum class HostedPageMode
	{
		kActivate,
		kCommit,
		kReadCurrent
	};

	class HostedPageOperation final : public std::enable_shared_from_this<HostedPageOperation>
	{
	public:
		using BusyCheck = std::function<bool()>;
		using Completion = std::function<void(Result<MCMPage>)>;

		HostedPageOperation(
			MCMDescriptor                   a_descriptor,
			std::shared_ptr<IClassicScript> a_script,
			std::string                     a_pageKey,
			std::int32_t                    a_pageIndex,
			bool                            a_configAlreadyOpen,
			HostedPageMode                  a_mode,
			BusyCheck                       a_busyCheck,
			Completion                      a_completion,
			IOperationTimer&                a_timer,
			const IOperationClock&          a_clock = SteadyOperationClock::GetSingleton());

		void Start();
		void SetExpectedPages(std::vector<ClassicPageSelection> a_pages);
		void SetMenuResolver(IClassicMenuOptionResolver& a_resolver) { menuResolver = &a_resolver; }
		void Cancel();
		void YieldToFrontend();
		bool IsConfigOpen() const;

	private:
		void Commit();
		void Open();
		void SetPage();
		void ReadPage();
		void ReadNextMetadata();
		bool ValidatePages();
		void Fail(BridgeError a_error);
		void Finish(Result<MCMPage> a_result);
		bool CheckBudget();
		bool Dispatch(ClassicCall a_call, std::function<void()> a_next);
		void Continue(std::uint64_t a_token, std::function<void()> a_next);
		void ArmTimeout(std::uint64_t a_token);

		MCMDescriptor                                    descriptor;
		std::shared_ptr<IClassicScript>                  script;
		std::string                                      pageKey;
		std::int32_t                                     pageIndex{};
		std::optional<std::vector<ClassicPageSelection>> expectedPages;
		IClassicMenuOptionResolver*                      menuResolver{};
		std::optional<MCMPage>                           page;
		std::size_t                                      metadataIndex{};
		BusyCheck                                        busyCheck;
		Completion                                       completion;
		IOperationTimer&                                 timer;
		OperationDeadline                                deadline{ timer };
		OperationContext                                 operation;
		HostedPageMode                                   mode;
		bool                                             configOpen{};
		bool                                             finished{};
	};
}
