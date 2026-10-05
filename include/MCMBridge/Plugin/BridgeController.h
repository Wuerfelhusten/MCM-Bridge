#pragma once

#include "MCMBridge/Core/CustomContentPolicy.h"
#include "MCMBridge/Core/NavigationMerge.h"

#include "MCMBridge/API/HostData.h"
#include "MCMBridge/API/MCMBridgeHost.h"
#include "MCMBridge/Core/ExternalOperationState.h"
#include "MCMBridge/Core/FacadeCallState.h"
#include "MCMBridge/Core/HostedPageRoute.h"
#include "MCMBridge/Core/MenuOptionResolver.h"
#include "MCMBridge/Core/ViewLoadState.h"
#include "MCMBridge/Discovery/AutomaticRegistryProvider.h"
#include "MCMBridge/Papyrus/ClassicHelpOperation.h"
#include "MCMBridge/Papyrus/ClassicScanOperation.h"
#include "MCMBridge/Papyrus/ClassicWriteOperation.h"
#include "MCMBridge/Papyrus/HostCallSession.h"
#include "MCMBridge/Papyrus/HostedCloseOperation.h"
#include "MCMBridge/Papyrus/HostedPageOperation.h"
#include "MCMBridge/Snapshot/SnapshotStore.h"
#include "MCMBridge/Write/WriteQueue.h"

#include <array>
#include <atomic>
#include <deque>
#include <functional>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace MCMBridge
{
	class BridgeController final : public IWriteDispatcher
	{
	public:
		static BridgeController& GetSingleton();
		MCMHostResult            BeginHostContext(std::string_view a_owner, bool a_restore, MCMHostContext& a_context);
		MCMHostResult            InvokeHost(MCMHostContext a_context, const MCMHostCall& a_call, MCMHostCompletion a_completion, void* a_user);
		MCMHostResult            EndHostContext(MCMHostContext a_context, bool a_cancel);
		std::optional<bool>      HostMessageResponse(bool a_confirmation);
		Result<HostDataInput>    ReadHostData(MCMHostContext a_context);
		std::uint64_t            UserEditID() const { return activeRecordingID; }
		void                     ObserveUserConfirmation(std::uint64_t a_edit, bool a_accepted);
		void                     BeginScriptCall(std::uint64_t a_session, std::int32_t a_request, RE::BSTSmartPointer<RE::BSScript::Object> a_script,
			RE::BSTSmartPointer<RE::BSScript::Stack> a_stack, std::string a_method);
		void                     FinishScriptCall(std::uint64_t a_session, std::int32_t a_request, std::int32_t a_admission, std::uint32_t a_stack, bool a_valid);

		void StartSession(std::string a_reason, bool a_discover = true);
		// Native bootstrap and registrations are dispatched on the game task queue.
		Result<bool>                       ActivateNativeHost(RE::BSTSmartPointer<RE::BSScript::Object> a_manager);
		Result<bool>                       ResetNativeHost(const RE::BSTSmartPointer<RE::BSScript::Object>& a_manager);
		NativeRegistryProvider&            NativeRegistry() { return registry.Native(); }
		void                               RequestRefresh(bool a_navigationOnly = false);
		void                               NotifyRegistryEvent();
		void                               NotifyFrontendInvalidation();
		void                               RequestControlHelp(SettingIdentity a_identity, MCMValue a_value);
		void                               RetryFailed();
		void                               RetryHostedPage();
		void                               Submit(WriteCommand a_command) override;
		std::shared_ptr<const MCMSnapshot> Snapshot() const;
		bool                               IsSessionReady() const { return sessionReady.load(); }
		// Thread-safe selection capture; execution and lease checks stay on the game queue.
		std::optional<std::uint64_t> CaptureScriptView(std::string_view a_modID) const;
		void                         QueueScriptPage(std::uint64_t a_session, std::uint64_t a_revision, std::string a_modID, std::string a_page, bool a_opening = false);
		void                         ObserveCustomContent(const MCMMod& a_mod, const MCMPage& a_page, CustomContentOrigin a_origin = CustomContentOrigin::kUser);
		bool                         IsRegistryAvailable() const;
		void                         BeginFrameworkFrame();
		void                         OpenFrameworkView();
		void                         CloseFrameworkView();
		bool                         IsHostedViewReady(std::string_view a_modID, std::string_view a_pageID) const;
		std::string                  HostedViewError() const;
		void                         ObserveHostedPage(std::string_view a_modID, std::string_view a_pageID);
		std::string                  ResolveHostedPage(std::string_view a_modID, std::string_view a_pageID);
		// Game task only, after the owned VM callback has completed.
		std::optional<std::uint64_t> ScriptViewRevision(std::uint64_t a_session, std::string_view a_modID);
		void                         CloseScriptView(std::uint64_t a_session, std::uint64_t a_revision, std::string_view a_modID, bool a_closeFrontend);
		void                         ClearHostedPageRoute();
		void                         EndFrameworkFrame();
		void                         CloseHostedView();
		void                         ObserveMenuOptions(
			std::string_view         a_menuName,
			std::string_view         a_target,
			std::vector<std::string> a_options);
		MCMHostResult BeginExternalOperation(std::string_view a_owner, std::uint64_t& a_leaseToken);
		MCMHostResult EndExternalOperation(std::uint64_t a_leaseToken);
		MCMHostResult CancelExternalOperation(std::string_view a_owner);

		void ResumeGameUI();

	private:
		void RetireScriptContext(bool a_timeout = false, bool a_resume = true);
		void CloseScriptContext();
		bool BorrowScriptContext(const RE::BSTSmartPointer<RE::BSScript::Object>& a_script, const std::string& a_mod, std::uint32_t a_stack);
		void ReturnScriptContext();
		void ReadyScriptCall(std::uint64_t a_session, std::int32_t a_request, std::uint64_t a_permit);
		struct ScriptContext
		{
			FacadeCallState                           calls;
			RE::BSTSmartPointer<RE::BSScript::Object> script;
			RE::BSTSmartPointer<RE::BSScript::Stack>  stack;
			std::uint64_t                             lease{};
			std::uint64_t                             timer{};
			std::unique_ptr<OperationDeadline>        executionDeadline;
			std::uint64_t                             pause{};
			bool                                      closing{};
			bool                                      borrowed{};
			std::uint64_t                             viewRevision{};
			std::string                               owner;
			std::string                               mod;
			struct Call
			{
				std::uint64_t permit;
				bool          closing;
			};
			std::unordered_map<std::int32_t, Call> admitted;
		};
		ScriptContext scriptContext;
		void          ReleaseHostContext();
		void          CloseHostContext();
		struct DirectContext
		{
			MCMHostContext                                                                           id{};
			std::uint64_t                                                                            lease{};
			std::string                                                                              owner;
			std::chrono::steady_clock::time_point                                                    started;
			std::array<std::uint64_t, static_cast<std::size_t>(ClassicMethod::kOnSettingChange) + 1> completedCalls{};
			std::uint64_t                                                                            failedCalls{};
			double                                                                                   callMilliseconds{};
			std::uint64_t                                                                            pause{};
			std::string                                                                              modID;
			std::string                                                                              stableID;
			std::shared_ptr<IClassicScript>                                                          script;
			std::shared_ptr<HostCallSession>                                                         calls;
			std::optional<ClassicCall>                                                               lastCall;
			bool                                                                                     restore{};
			bool                                                                                     ended{};
			bool                                                                                     cancelled{};
			bool                                                                                     acceptConfirmation{};
			bool                                                                                     confirmationDeclined{};
			bool                                                                                     closing{};
		};
		DirectContext       directContext;
		std::uint64_t       nextHostContext{ 1 };
		bool                AdoptHostedPage(std::uint64_t& a_revision, const std::string& a_modID, std::string& a_pageID, const MCMPage& a_page);
		HostedPageRoute     hostedPageRoute;
		CustomContentVisits customVisits;
		bool                AllowsRefresh() const;
		void                QueueNavigationPoll();
		void                PollNavigation();
		void                RefreshOnGameThread(bool a_registryCheck);
		void                QueueRegistryCheck();
		void                BeginScan(std::vector<LiveMCM> a_entries);
		void                ReadNativeNavigation();
		void                ScanNext(std::uint64_t a_session);
		void                FinishScan(std::uint64_t a_session);
		void                ProcessWrites();
		void                DeferWrite(WriteCommand a_command, std::chrono::milliseconds a_delay);
		void                RejectWrite(const WriteCommand& a_command, const BridgeError& a_error);
		void                NotifyWriteTimeout(const WriteCommand& a_command, const BridgeError& a_error);
		void                StartControlHelp(SettingIdentity a_identity, std::string a_requestKey, std::uint64_t a_session);
		void                FinishWrite(const WriteCommand& a_command, const Result<MCMValue>& a_result, bool a_hosted);
		void                ScheduleHostedRequest(std::string a_modID, std::string a_pageID);
		void                QueueHostedDrive();
		void                DriveHostedPage();
		void                RecoverHostedNavigation(std::uint64_t a_revision, const std::string& a_modID, const std::string& a_pageID);
		void                StartHostedPageOperation();
		void                FinishHostedPage(
			std::uint64_t   a_session,
			std::uint64_t   a_revision,
			std::string     a_modID,
			std::string     a_pageID,
			Result<MCMPage> a_result);
		using HostedWriteNavigation = std::optional<std::pair<std::uint64_t, ClassicPageSelection>>;
		bool RouteCommittedPage(const std::string& a_modID, const std::string& a_pageID, const HostedWriteNavigation& a_navigation);
		void StartHostedCommit(WriteCommand a_command, MCMValue a_confirmedValue, HostedWriteNavigation a_navigation);
		void FinishHostedCommit(
			std::uint64_t         a_session,
			std::string           a_modID,
			std::string           a_pageID,
			WriteCommand          a_command,
			MCMValue              a_confirmedValue,
			Result<MCMPage>       a_result,
			HostedWriteNavigation a_navigation);
		void PublishHostedPage(std::string_view a_modID, std::string_view a_pageID, MCMPage a_page, bool a_freshMetadata = false);
		void ScheduleHostedAutoRefresh();
		void RunHostedAutoRefresh(std::uint64_t a_token, std::size_t a_pass);
		void FinishHostedAutoRefresh(
			std::uint64_t   a_session,
			std::uint64_t   a_token,
			std::size_t     a_pass,
			Result<MCMPage> a_result);
		void                   CloseHostedSession(std::function<void()> a_continuation = {});
		void                   ClearHostedState();
		void                   RunHostedCloseContinuations();
		void                   AbandonHostedSession(bool a_clearRequest);
		bool                   HasHostedSession() const;
		bool                   IsHostedTarget(const SettingIdentity& a_identity) const;
		bool                   IsDesiredHostedTarget(const SettingIdentity& a_identity) const;
		std::optional<LiveMCM> FindLive(const SettingIdentity& a_identity) const;
		MCMMod                 MergeHelper(MCMMod a_liveMod) const;
		bool                   ExternalOperationBlocked() const;
		void                   BeginCaptureSession();
		void                   UpdateCaptureSession();
		void                   EndCaptureSession();

		AutomaticRegistryProvider registry;
		// Last successfully observed registration identities; navigation polling owns pages.
		std::vector<std::string> registryIDs;
		struct ScriptPageRequest
		{
			std::uint64_t session{};
			std::uint64_t revision{};
			std::string   mod;
			std::string   page;
			bool          opening{};
		};
		void                                   ResolveScriptPage();
		std::optional<ScriptPageRequest>       scriptPageRequest;
		ViewLoadState                          viewLoad;
		std::atomic_bool                       frameworkViewOpen{};
		std::atomic_bool                       navigationPollQueued{};
		std::atomic_uint64_t                   navigationEpoch{};
		std::atomic_int64_t                    nextNavigationPoll{};
		ScopedMenuOptionResolver               menuResolver;
		SnapshotStore                          snapshots;
		WriteQueue                             writes;
		std::shared_ptr<WriteTiming>           activeWriteTiming;
		std::vector<LiveMCM>                   liveEntries;
		MCMSnapshot                            pendingSnapshot;
		std::shared_ptr<ClassicScanOperation>  activeScan;
		std::shared_ptr<ClassicWriteOperation> activeWrite;
		std::shared_ptr<ClassicHelpOperation>  activeHelp;
		std::shared_ptr<IClassicScript>        hostedScript;
		std::shared_ptr<HostedPageOperation>   activeHostedPage;
		std::shared_ptr<HostedCloseOperation>  activeHostedClose;
		MCMDescriptor                          hostedDescriptor;
		std::string                            hostedPageID;
		std::string                            hostedPageKey;
		std::int32_t                           hostedPageIndex{ -1 };
		std::vector<std::function<void()>>     hostedCloseContinuations;
		mutable std::mutex                     hostedRequestMutex;
		mutable std::mutex                     helpMutex;
		mutable std::mutex                     externalOperationMutex;
		std::string                            requestedHostedModID;
		std::string                            requestedHostedPageID;
		NavigationRecovery                     navigationRecovery;
		std::unordered_set<std::string>        quarantined;
		std::unordered_set<std::string>        pendingHelp;
		std::unordered_set<std::string>        resolvedHelp;
		std::size_t                            scanIndex{};
		std::uint64_t                          session{};
		std::uint64_t                          hostedRefreshToken{};
		ExternalOperationState                 externalOperation;
		std::uint64_t                          captureSessionID{};
		std::uint64_t                          nextRecordingID{};
		std::uint64_t                          activeRecordingID{};
		bool                                   recordingAccepted{};
		bool                                   recordingDeclined{};
		bool                                   captureSessionActive{};
		std::atomic_bool                       sessionReady{};
		std::atomic_bool                       refreshRequested{};
		std::atomic_bool                       fullRefreshRequested{};
		std::atomic_bool                       registryEventQueued{};
		std::atomic_bool                       registryCheckPending{};
		std::atomic_bool                       hostedDriveQueued{};
		std::atomic_bool                       hostedRenderedThisFrame{};
		std::atomic_bool                       frontendInvalidationQueued{};
		bool                                   hostedReady{};
		bool                                   activeWriteHosted{};
		bool                                   refreshing{};
		bool                                   writeRetryScheduled{};
	};
}
