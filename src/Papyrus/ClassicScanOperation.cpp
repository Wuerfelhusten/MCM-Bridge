#include "MCMBridge/Papyrus/ClassicScanOperation.h"

#include "MCMBridge/Core/StableId.h"
#include <format>

namespace
{
	constexpr auto callTimeout = std::chrono::seconds(10);
	constexpr auto operationBudget = std::chrono::seconds(60);
}

namespace MCMBridge
{
	ClassicScanOperation::ClassicScanOperation(
		MCMDescriptor                   a_descriptor,
		std::shared_ptr<IClassicScript> a_script,
		IClassicMenuOptionResolver&     a_menuResolver,
		BusyCheck                       a_busyCheck,
		Completion                      a_completion,
		IOperationTimer&                a_timer,
		const IOperationClock&          a_clock) :
		script(std::move(a_script)),
		menuResolver(a_menuResolver),
		busyCheck(std::move(a_busyCheck)),
		completion(std::move(a_completion)),
		timer(a_timer),
		operation(operationBudget, a_clock, [this] { return script->MessageWaitDuration(); })
	{
		mod.stableID = a_descriptor.stableID;
		mod.displayName = a_descriptor.displayName;
		mod.backend = a_descriptor.backend;
		mod.ownerPlugin = a_descriptor.ownerPlugin;
		mod.questFormID = a_descriptor.questFormID;
		mod.scriptName = a_descriptor.scriptName;
		mod.interopID = a_descriptor.interopID;
		mod.pageScopedState = a_descriptor.pageScopedState;
	}

	void ClassicScanOperation::Start()
	{
		operation.Start();
		if (busyCheck && busyCheck()) {
			Finish(std::unexpected(BridgeError{ BridgeErrorCode::kBusy, "Journal MCM is active" }));
			return;
		}
		if (configOpen && script->IsConfigOpen())
			ReadPages();
		else
			Open();
	}

	void ClassicScanOperation::Cancel()
	{
		if (!finished) {
			Fail(BridgeError{ BridgeErrorCode::kStaleSnapshot, "Scan was cancelled" }, configOpen);
		}
	}

	void ClassicScanOperation::YieldToFrontend()
	{
		if (!finished) {
			Finish(std::unexpected(BridgeError{ BridgeErrorCode::kBusy, "Journal MCM took ownership" }));
		}
	}

	void ClassicScanOperation::Open()
	{
		openedHere = true;
		if (reuseCurrentPage)
			script->BeginPageCapture();
		Dispatch({ .method = ClassicMethod::kOpenConfig }, [self = shared_from_this()] {
			self->configOpen = self->script->IsConfigOpen();
			if (!self->configOpen) {
				self->Fail(BridgeError{ BridgeErrorCode::kInvalidData, "OpenConfig did not initialize SkyUI buffers" }, false);
				return;
			}
			self->ReadPages();
		});
	}

	void ClassicScanOperation::ReadPages()
	{
		if (busyCheck && busyCheck()) {
			YieldToFrontend();
			return;
		}
		const auto navigation = script->ReadNavigationPages();
		if (!navigation) {
			Fail({ BridgeErrorCode::kInvalidData, "MCM navigation is unavailable" }, true);
			return;
		}
		const auto&                         pageNames = *navigation;
		std::optional<ClassicPageSelection> openingPage;
		if (mod.pageScopedState) {
			openingPage = script->ReadCurrentPage();
			if (!openingPage) {
				Fail(BridgeError{ BridgeErrorCode::kInvalidData, "NL_MCM opening page is unavailable" }, true);
				return;
			}
		}
		pages = BuildClassicPageList(pageNames, std::move(openingPage));
		if (navigationOnly) {
			for (const auto& selection : pages) {
				MCMPage page;
				page.stableID = MakeClassicPageID(mod.stableID, selection.name, selection.index);
				page.rawName = selection.name;
				page.displayName = selection.name;
				page.index = selection.index;
				mod.pages.push_back(std::move(page));
			}
			Close();
			return;
		}
		pageIndex = 0;
		SetPage();
	}

	void ClassicScanOperation::SetPage()
	{
		while (pageIndex < pages.size() && pageFilter && !pageFilter(pages[pageIndex])) {
			const auto& selection = pages[pageIndex++];
			MCMPage     page;
			page.stableID = MakeClassicPageID(mod.stableID, selection.name, selection.index);
			page.rawName = selection.name;
			page.displayName = selection.name;
			page.index = selection.index;
			mod.pages.push_back(std::move(page));
		}
		if (pageIndex >= pages.size()) {
			Close();
			return;
		}
		if (busyCheck && busyCheck()) {
			YieldToFrontend();
			return;
		}

		const auto& page = pages[pageIndex];
		const auto  current = script->ReadCurrentPage();
		if (reuseCurrentPage && (!openedHere || script->CanReuseOpeningPage()) && script->CanReusePage() && current && current->name == page.name && current->index == page.index && script->IsPageReady(page.index)) {
			ReadPage();
			return;
		}
		openedHere = false;
		script->BeginPageCapture();
		Dispatch(
			{ .method = ClassicMethod::kSetPage, .text = page.name, .integer = page.index },
			[self = shared_from_this()] { self->ReadPage(); });
	}

	void ClassicScanOperation::ReadPage()
	{
		const auto& currentPage = pages[pageIndex];
		if (!script->IsPageReady(currentPage.index)) {
			Fail(BridgeError{ BridgeErrorCode::kInvalidData, "SetPage did not produce a ready page" }, true);
			return;
		}

		ClassicPageContext context{
			.modID = mod.stableID,
			.ownerPlugin = mod.ownerPlugin,
			.questFormID = mod.questFormID,
			.scriptName = mod.scriptName,
			.pageName = currentPage.name,
			.pageIndex = currentPage.index
		};
		auto page = script->ReadPage(context);
		if (!page) {
			Fail(page.error(), true);
			return;
		}
		mod.pages.push_back(pageTransform ? pageTransform(std::move(*page)) : std::move(*page));
		mod.pages.back().title = script->ReadPageTitle();
		if (pageObserver)
			pageObserver(mod.pages.back(), *script);
		metadataIndex = 0;
		ReadNextMetadata();
	}

	void ClassicScanOperation::AdvancePage()
	{
		++pageIndex;
		SetPage();
	}

	void ClassicScanOperation::Close()
	{
		if (keepOpen && !pendingError) {
			Finish(std::move(mod));
			return;
		}
		if (!configOpen) {
			if (pendingError) {
				Finish(std::unexpected(std::move(*pendingError)));
			} else {
				Finish(std::move(mod));
			}
			return;
		}

		closing = true;
		Dispatch({ .method = ClassicMethod::kCloseConfig }, [self = shared_from_this()] {
			self->configOpen = false;
			self->closing = false;
			if (self->pendingError) {
				self->Finish(std::unexpected(std::move(*self->pendingError)));
			} else {
				self->Finish(std::move(self->mod));
			}
		});
	}

	void ClassicScanOperation::Fail(BridgeError a_error, bool a_closeConfig)
	{
		if (finished) {
			return;
		}
		pendingError = std::move(a_error);
		if (a_closeConfig && configOpen) {
			Close();
		} else {
			Finish(std::unexpected(std::move(*pendingError)));
		}
	}

	void ClassicScanOperation::Finish(Result<MCMMod> a_result)
	{
		if (finished) {
			return;
		}
		finished = true;
		deadline.Cancel();
		menuResolver.CancelCapture();
		operation.Invalidate();
		if (!a_result && a_result.error().code == BridgeErrorCode::kTimedOut)
			script->RetireExecution();
		if (completion) {
			completion(std::move(a_result));
		}
	}

	bool ClassicScanOperation::CheckBudget()
	{
		if (!operation.IsExpired()) {
			return true;
		}
		Fail(BridgeError{ BridgeErrorCode::kTimedOut, "MCM scan exceeded its operation budget" }, false);
		return false;
	}

	bool ClassicScanOperation::Dispatch(
		ClassicCall           a_call,
		std::function<void()> a_next)
	{
		if (finished || !CheckBudget()) {
			return false;
		}
		if (a_call.method != ClassicMethod::kCloseConfig && busyCheck && busyCheck()) {
			YieldToFrontend();
			return false;
		}
		const auto token = operation.BeginStep();
		const auto methodName = ClassicMethodName(a_call.method);
		const auto method = a_call.method;
		const auto dispatched = script->Dispatch(std::move(a_call), [weak = weak_from_this(), token, next = std::move(a_next)]() mutable {
			if (auto self = weak.lock()) {
				self->Continue(token, std::move(next));
			}
		});
		if (!dispatched) {
			BridgeError error{ BridgeErrorCode::kDispatchFailed, std::format("Papyrus call {} could not be dispatched", methodName) };
			if (closing) {
				configOpen = false;
				closing = false;
				if (pendingError) {
					Finish(std::unexpected(std::move(*pendingError)));
				} else {
					Finish(std::unexpected(std::move(error)));
				}
			} else {
				Fail(std::move(error), configOpen);
			}
			return false;
		}
		if (!finished && operation.IsCurrent(token)) {
			ArmTimeout(token, method);
		}
		return true;
	}

	void ClassicScanOperation::Continue(std::uint64_t a_token, std::function<void()> a_next)
	{
		if (finished || !operation.IsCurrent(a_token)) {
			return;
		}
		deadline.Cancel();
		if (!CheckBudget()) {
			return;
		}
		if (busyCheck && busyCheck()) {
			YieldToFrontend();
			return;
		}
		if (!closing && continuationGuard && !continuationGuard()) {
			configOpen = script->IsConfigOpen();
			Fail(BridgeError{ BridgeErrorCode::kUnavailable, "Restore preparation was cancelled" }, true);
			return;
		}
		if (a_next) {
			a_next();
		}
	}

	void ClassicScanOperation::ArmTimeout(std::uint64_t a_token, ClassicMethod a_method)
	{
		deadline.Arm(callTimeout, [weak = weak_from_this(), a_token, a_method] {
			if (auto self = weak.lock(); self && !self->finished && self->operation.IsCurrent(a_token)) {
				self->Fail(BridgeError{ BridgeErrorCode::kTimedOut, std::format("Papyrus call {} timed out; Papyrus may still finish", ClassicMethodName(a_method)) }, false);
			} }, [target = script] { return target->MessageWaitDuration(); });
	}
}
