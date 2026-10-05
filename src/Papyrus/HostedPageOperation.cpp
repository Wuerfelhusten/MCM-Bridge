#include "MCMBridge/Papyrus/HostedPageOperation.h"

#include <algorithm>
#include <format>

namespace
{
	constexpr auto callTimeout = std::chrono::seconds(10);
	constexpr auto operationBudget = std::chrono::seconds(30);
}

namespace MCMBridge
{
	HostedPageOperation::HostedPageOperation(
		MCMDescriptor                   a_descriptor,
		std::shared_ptr<IClassicScript> a_script,
		std::string                     a_pageKey,
		std::int32_t                    a_pageIndex,
		bool                            a_configAlreadyOpen,
		HostedPageMode                  a_mode,
		BusyCheck                       a_busyCheck,
		Completion                      a_completion,
		IOperationTimer&                a_timer,
		const IOperationClock&          a_clock) :
		descriptor(std::move(a_descriptor)),
		script(std::move(a_script)),
		pageKey(std::move(a_pageKey)),
		pageIndex(a_pageIndex),
		busyCheck(std::move(a_busyCheck)),
		completion(std::move(a_completion)),
		timer(a_timer),
		operation(operationBudget, a_clock, [this] { return script->MessageWaitDuration(); }),
		mode(a_mode),
		configOpen(a_configAlreadyOpen)
	{}

	void HostedPageOperation::Start()
	{
		operation.Start();
		if (busyCheck && busyCheck()) {
			Fail({ BridgeErrorCode::kBusy, "Journal MCM is active" });
			return;
		}
		configOpen = configOpen && script->IsConfigOpen();
		if (mode == HostedPageMode::kReadCurrent) {
			const auto current = script->ReadCurrentPage();
			if (!configOpen || !current || *current != ClassicPageSelection{ pageKey, pageIndex }) {
				Fail({ BridgeErrorCode::kStaleSnapshot, "Hosted page no longer matches the completed external call" });
				return;
			}
			// The external caller already built this page. Never repeat its callbacks.
			ReadPage();
			return;
		}
		if (mode == HostedPageMode::kCommit) {
			if (!configOpen) {
				Fail({ BridgeErrorCode::kUnavailable, "Hosted MCM config is not open for commit" });
				return;
			}
			Commit();
			return;
		}
		if (configOpen) {
			SetPage();
		} else {
			Open();
		}
	}

	void HostedPageOperation::Commit()
	{
		Dispatch({ .method = ClassicMethod::kCloseConfig }, [self = shared_from_this()] {
			self->configOpen = false;
			self->Open();
		});
	}

	void HostedPageOperation::Cancel()
	{
		if (!finished) {
			Fail({ BridgeErrorCode::kStaleSnapshot, "Hosted page activation was cancelled" });
		}
	}

	void HostedPageOperation::YieldToFrontend()
	{
		Fail({ BridgeErrorCode::kBusy, "Journal MCM took ownership" });
	}

	bool HostedPageOperation::IsConfigOpen() const
	{
		return configOpen && script->IsConfigOpen();
	}

	void HostedPageOperation::Open()
	{
		Dispatch({ .method = ClassicMethod::kOpenConfig }, [self = shared_from_this()] {
			self->configOpen = self->script->IsConfigOpen();
			if (!self->configOpen) {
				self->Fail({ BridgeErrorCode::kInvalidData, "OpenConfig did not initialize SkyUI buffers" });
				return;
			}
			self->SetPage();
		});
	}

	void HostedPageOperation::SetPage()
	{
		if (!ValidatePages())
			return;
		script->BeginPageCapture();
		Dispatch(
			{ .method = ClassicMethod::kSetPage, .text = pageKey, .integer = pageIndex },
			[self = shared_from_this()] { self->ReadPage(); });
	}

	void HostedPageOperation::ReadPage()
	{
		if (mode == HostedPageMode::kActivate) {
			if (auto target = script->TakePageRedirect()) {
				pageKey = std::move(target->name);
				pageIndex = target->index;
			}
		}
		if (!ValidatePages())
			return;
		if (!script->IsPageReady(pageIndex)) {
			Fail({ BridgeErrorCode::kInvalidData, "SetPage did not produce a ready hosted page" });
			return;
		}
		ClassicPageContext context{
			.modID = descriptor.stableID,
			.ownerPlugin = descriptor.ownerPlugin,
			.questFormID = descriptor.questFormID,
			.scriptName = descriptor.scriptName,
			.pageName = pageKey,
			.pageIndex = pageIndex
		};
		auto loadedPage = script->ReadPage(context);
		if (loadedPage) {
			loadedPage->title = script->ReadPageTitle();
		}
		if (loadedPage && menuResolver) {
			page = std::move(*loadedPage);
			ReadNextMetadata();
			return;
		}
		Finish(std::move(loadedPage));
	}

	void HostedPageOperation::SetExpectedPages(std::vector<ClassicPageSelection> a_pages)
	{
		std::erase_if(a_pages, [](const auto& a_page) { return a_page.index < 0; });
		std::ranges::sort(a_pages, {}, &ClassicPageSelection::index);
		expectedPages = std::move(a_pages);
	}

	bool HostedPageOperation::ValidatePages()
	{
		const auto navigation = script->ReadNavigationPages();
		if (!navigation) {
			Fail({ BridgeErrorCode::kInvalidData, "MCM navigation is unavailable" });
			return false;
		}
		auto pages = BuildClassicPageList(*navigation);
		std::erase_if(pages, [](const auto& a_page) { return a_page.index < 0; });
		const auto targetValid = pageIndex < 0 ||
		                         std::ranges::find(pages, ClassicPageSelection{ pageKey, pageIndex }) != pages.end();
		if (!targetValid || (expectedPages && *expectedPages != pages)) {
			Fail({ BridgeErrorCode::kStaleSnapshot, "MCM page list changed; navigation must be rebuilt" });
			return false;
		}
		return true;
	}

	void HostedPageOperation::Fail(BridgeError a_error)
	{
		Finish(std::unexpected(std::move(a_error)));
	}

	void HostedPageOperation::Finish(Result<MCMPage> a_result)
	{
		if (finished) {
			return;
		}
		finished = true;
		deadline.Cancel();
		if (menuResolver)
			menuResolver->CancelCapture();
		operation.Invalidate();
		// Revoke host ownership before completion can admit another MCM. The VM
		// may retain this adapter through its callback; do not rely on destruction.
		if (!a_result && a_result.error().code == BridgeErrorCode::kTimedOut)
			script->RetireExecution();
		if (completion) {
			completion(std::move(a_result));
		}
	}

	bool HostedPageOperation::CheckBudget()
	{
		if (!operation.IsExpired()) {
			return true;
		}
		Fail({ BridgeErrorCode::kTimedOut, "Hosted page activation exceeded its operation budget" });
		return false;
	}

	bool HostedPageOperation::Dispatch(ClassicCall a_call, std::function<void()> a_next)
	{
		if (finished || !CheckBudget()) {
			return false;
		}
		if (busyCheck && busyCheck()) {
			YieldToFrontend();
			return false;
		}
		const auto token = operation.BeginStep();
		const auto methodName = ClassicMethodName(a_call.method);
		const auto dispatched = script->Dispatch(
			std::move(a_call),
			[weak = weak_from_this(), token, next = std::move(a_next)]() mutable {
				if (const auto self = weak.lock()) {
					self->Continue(token, std::move(next));
				}
			});
		if (!dispatched) {
			Fail({ BridgeErrorCode::kDispatchFailed, std::format("Papyrus call {} could not be dispatched", methodName) });
			return false;
		}
		if (!finished && operation.IsCurrent(token)) {
			ArmTimeout(token);
		}
		return true;
	}

	void HostedPageOperation::Continue(std::uint64_t a_token, std::function<void()> a_next)
	{
		if (finished || !operation.IsCurrent(a_token) || !CheckBudget()) {
			return;
		}
		deadline.Cancel();
		if (busyCheck && busyCheck()) {
			YieldToFrontend();
			return;
		}
		if (a_next) {
			a_next();
		}
	}

	void HostedPageOperation::ArmTimeout(std::uint64_t a_token)
	{
		deadline.Arm(callTimeout, [weak = weak_from_this(), a_token] {
			if (const auto self = weak.lock(); self && !self->finished && self->operation.IsCurrent(a_token)) {
				self->Fail({ BridgeErrorCode::kTimedOut, "Papyrus call timed out" });
			} }, [target = script] { return target->MessageWaitDuration(); });
	}
}
