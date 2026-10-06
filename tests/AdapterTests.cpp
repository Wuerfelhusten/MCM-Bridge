#include "MCMBridge/Core/Interfaces.h"
#include "MCMBridge/Core/MenuOptionResolver.h"
#include "MCMBridge/Core/NativeHostSession.h"
#include "MCMBridge/Core/OperationTimer.h"
#include "MCMBridge/Papyrus/ClassicHelpOperation.h"
#include "MCMBridge/Papyrus/ClassicScanOperation.h"
#include "MCMBridge/Papyrus/ClassicWriteOperation.h"
#include "MCMBridge/Papyrus/HostCallSession.h"
#include "MCMBridge/Papyrus/HostedCloseOperation.h"
#include "MCMBridge/Papyrus/HostedPageOperation.h"
#include "MCMBridge/Papyrus/IMCMHostAdapter.h"
#include "MCMBridge/Papyrus/RegisteredNavigation.h"
#include "MCMBridge/Write/WritePauseState.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <deque>
#include <map>
#include <optional>
#include <utility>

TEST_CASE("Native registered navigation reads 2233 menus without acquiring execution sessions")
{
	class RegisteredAdapter final : public MCMBridge::IMCMHostAdapter
	{
	public:
		std::shared_ptr<MCMBridge::IClassicScript> CreateSession() const override
		{
			++sessions;
			return {};
		}
		std::optional<std::vector<std::string>> ReadRegisteredPages() const override
		{
			++reads;
			return pages;
		}
		std::optional<std::vector<std::string>> pages{ std::vector<std::string>{ "General", "Hall (1/4)" } };
		mutable std::size_t                     reads{};
		mutable std::size_t                     sessions{};
	} adapter;
	for (int index = 0; index < 2233; ++index) {
		MCMBridge::MCMMod mod;
		mod.stableID = std::to_string(index);
		REQUIRE(MCMBridge::ReadRegisteredNavigation(adapter, mod));
		REQUIRE(mod.pages.size() == 2);
		CHECK(mod.pages[1].rawName == "Hall (1/4)");
		CHECK(mod.pages[1].controls.empty());
	}
	CHECK(adapter.reads == 2233);
	CHECK(adapter.sessions == 0);
	MCMBridge::MCMMod mod;
	mod.stableID = "existing";
	REQUIRE(MCMBridge::ReadRegisteredNavigation(adapter, mod));
	mod.pages[0].title = "confirmed";
	adapter.pages.reset();
	CHECK_FALSE(MCMBridge::ReadRegisteredNavigation(adapter, mod));
	REQUIRE(mod.pages.size() == 2);
	CHECK(mod.pages[0].title == "confirmed");
	mod.pages.clear();
	CHECK_FALSE(MCMBridge::ReadRegisteredNavigation(adapter, mod));
	REQUIRE(mod.pages.size() == 1);
	CHECK(mod.pages[0].index == -1);
	CHECK(mod.pages[0].openingPlaceholder);
	adapter.pages = std::vector<std::string>{};
	REQUIRE(MCMBridge::ReadRegisteredNavigation(adapter, mod));
	CHECK(mod.pages[0].index == -1);
	adapter.pages = std::vector<std::string>{ "Created on open" };
	REQUIRE(MCMBridge::ReadRegisteredNavigation(adapter, mod));
	REQUIRE(mod.pages.size() == 1);
	CHECK(mod.pages[0].rawName == "Created on open");
	CHECK_FALSE(mod.pages[0].openingPlaceholder);
	CHECK(adapter.sessions == 0);
}

namespace
{
	class FakeRegistry final : public MCMBridge::IMCMRegistryProvider
	{
	public:
		MCMBridge::Result<std::vector<MCMBridge::MCMDescriptor>> Read() override
		{
			if (error) {
				return std::unexpected(*error);
			}
			return entries;
		}

		bool IsBusy() const override
		{
			return busy;
		}

		std::vector<MCMBridge::MCMDescriptor> entries;
		std::optional<MCMBridge::BridgeError> error;
		bool                                  busy{};
	};

	class FakeBackend final : public MCMBridge::IMCMBackend
	{
	public:
		void BuildSnapshot(const MCMBridge::MCMDescriptor& a_descriptor, MCMBridge::SnapshotCompletion a_completion) override
		{
			MCMBridge::MCMMod mod;
			mod.stableID = a_descriptor.stableID;
			a_completion(std::move(mod));
		}

		void EnqueueWrite(MCMBridge::WriteCommand a_command, MCMBridge::WriteCompletion a_completion) override
		{
			a_completion(std::move(a_command.desiredValue));
		}
	};

	class FakeTimer final : public MCMBridge::IOperationTimer
	{
	public:
		void After(std::chrono::milliseconds a_delay, std::function<void()> a_task) override
		{
			if (a_delay.count() == 0) {
				ready.push_back(std::move(a_task));
				return;
			}
			tasks.push_back(std::move(a_task));
		}

		void RunNext()
		{
			auto task = std::move(tasks.front());
			tasks.pop_front();
			task();
		}

		std::deque<std::function<void()>> tasks;
		std::deque<std::function<void()>> ready;
	};

	class FakeScript final : public MCMBridge::IClassicScript
	{
	public:
		std::chrono::steady_clock::duration messageWait{};
		std::chrono::steady_clock::duration MessageWaitDuration() const override { return messageWait; }
		bool                                retired{};
		std::function<void()>               onRetire;
		void                                RetireExecution() override
		{
			if (std::exchange(retired, true))
				return;
			if (onRetire)
				onRetire();
		}
		bool Dispatch(MCMBridge::ClassicCall a_call, Continuation a_continuation) override
		{
			if (retired)
				return false;
			calls.push_back(a_call.method);
			if (failedMethod && *failedMethod == a_call.method) {
				return false;
			}

			auto complete = [this, method = a_call.method, text = a_call.text, number = a_call.number, integer = a_call.integer,
								secondaryInteger = a_call.secondaryInteger, continuation = std::move(a_continuation)]() mutable {
				if (method == MCMBridge::ClassicMethod::kOpenConfig) {
					configOpen = true;
				}
				if (trackCurrentPage && method == MCMBridge::ClassicMethod::kSetPage)
					currentPage = MCMBridge::ClassicPageSelection{ text, integer };
				if (method == MCMBridge::ClassicMethod::kSelectOption) {
					if (selectResult) {
						currentValue = *selectResult;
					} else if (const auto* current = std::get_if<bool>(&currentValue)) {
						currentValue = !*current;
					}
				}
				if (method == MCMBridge::ClassicMethod::kResetOption && resetResult) {
					currentValue = *resetResult;
				}
				if (method == MCMBridge::ClassicMethod::kSetSliderValue) {
					currentValue = number;
				}
				if (method == MCMBridge::ClassicMethod::kSetMenuIndex) {
					currentValue = integer;
					if (menuMetadata)
						menuMetadata->selectedIndex = integer;
				}
				if (method == MCMBridge::ClassicMethod::kSetColorValue) {
					colorMetadata->start = static_cast<std::uint32_t>(integer);
					currentValue = colorMetadata->start;
				}
				if (method == MCMBridge::ClassicMethod::kSetInputText) {
					currentValue = text;
				}
				if (method == MCMBridge::ClassicMethod::kRemapKey) {
					currentValue = secondaryInteger;
				}
				if (method == MCMBridge::ClassicMethod::kCloseConfig) {
					configOpen = false;
				}
				if (onComplete)
					onComplete(method);
				if (continuation) {
					continuation();
				}
			};
			if (deferCallbacks) {
				callbacks.push_back(std::move(complete));
			} else {
				complete();
			}
			return true;
		}

		std::vector<std::string> ReadPages() const override
		{
			return pages;
		}
		std::optional<std::vector<std::string>> ReadNavigationPages() const override
		{
			return missingNavigation ? std::nullopt : std::optional{ pages };
		}
		std::uint64_t                PageRevision() const override { return revision; }
		std::uint64_t                IdentityRevision() const override { return identityRevision.value_or(revision); }
		std::optional<std::uint64_t> identityRevision;
		std::size_t                  identityCaptures{};
		void                         BeginIdentityCapture() override { ++identityCaptures; }
		std::string                  requiredModID;
		bool                         CanReusePage() const override { return reusablePage; }
		bool                         CanReuseOpeningPage() const override { return capturedOpeningPage; }
		bool                         capturedOpeningPage{};
		bool                         reusablePage{ true };
		bool                         missingNavigation{};
		std::uint64_t                revision{};

		std::optional<MCMBridge::ClassicPageSelection> ReadCurrentPage() const override
		{
			return currentPage;
		}
		std::optional<MCMBridge::MCMControl>                                           selectionControl;
		std::optional<MCMBridge::MCMControl>                                           ReadSelectionControl(std::int32_t) const override { return selectionControl; }
		std::optional<MCMBridge::ClassicPageSelection>                                 pageRedirect;
		std::optional<MCMBridge::ClassicPageSelection>                                 TakePageRedirect() override { return std::exchange(pageRedirect, std::nullopt); }
		bool                                                                           trackCurrentPage{};
		std::function<void(MCMBridge::MCMPage&, const MCMBridge::ClassicPageContext&)> pageTransform;

		MCMBridge::Result<MCMBridge::MCMPage> ReadPage(const MCMBridge::ClassicPageContext& a_context) const override
		{
			if (!requiredModID.empty() && a_context.modID != requiredModID)
				return std::unexpected(MCMBridge::BridgeError{ MCMBridge::BridgeErrorCode::kInvalidData, "Native host page is not current" });
			readPages.push_back({ a_context.pageName, a_context.pageIndex });
			if (missingPageBuffers) {
				return std::unexpected(MCMBridge::BridgeError{ MCMBridge::BridgeErrorCode::kInvalidData, "Missing page buffers" });
			}
			auto result = page;
			if (livePage) {
				result.rawName = a_context.pageName;
				result.index = a_context.pageIndex;
				for (auto& control : result.controls) control.value = currentValue;
			}
			if (pageTransform)
				pageTransform(result, a_context);
			return result;
		}

		MCMBridge::Result<MCMBridge::SliderMetadata> ReadSliderMetadata(std::uint16_t) const override
		{
			auto result = sliderMetadata;
			if (result && livePage && !independentSliderStart && std::holds_alternative<float>(currentValue))
				result->start = std::get<float>(currentValue);
			return result;
		}
		bool independentSliderStart{};

		MCMBridge::Result<MCMBridge::MenuMetadata> ReadMenuMetadata(std::uint16_t) const override
		{
			auto result = menuMetadata;
			if (result && menuBufferOnly) {
				result->options.clear();
				result->availability = MCMBridge::MetadataAvailability::kMissing;
			}
			return result;
		}

		MCMBridge::Result<MCMBridge::ColorMetadata> ReadColorMetadata(std::uint16_t) const override
		{
			return colorMetadata;
		}

		MCMBridge::Result<MCMBridge::InputMetadata> ReadInputMetadata(std::uint16_t) const override
		{
			return inputMetadata;
		}

		std::optional<MCMBridge::MCMValue> ReadValue(MCMBridge::MCMControlType, std::uint16_t) const override
		{
			return currentValue;
		}

		bool          matches{ true };
		std::uint64_t resetRevision{};
		std::uint64_t PageResetRevision() const override { return resetRevision; }
		bool          Matches(const MCMBridge::SettingIdentity&, MCMBridge::MCMControlType) const override
		{
			return matches;
		}

		std::optional<std::int32_t> ReadOptionVariable(std::string_view a_name, std::int32_t a_pageIndex) const override
		{
			if (optionVariableLookup)
				return optionVariableLookup(a_name, a_pageIndex);
			return a_name == "cyclerOption" && a_pageIndex == 0 ? std::optional<std::int32_t>{ 0 } : std::nullopt;
		}
		std::function<std::optional<std::int32_t>(std::string_view, std::int32_t)> optionVariableLookup;

		bool IsConfigOpen() const override
		{
			return configOpen;
		}

		bool IsPageReady(std::int32_t) const override
		{
			return pageReady;
		}

		std::string ReadPageTitle() const override
		{
			return pageTitle;
		}

		std::string ReadInfoText() const override
		{
			return infoText;
		}

		void CompleteNext()
		{
			auto callback = std::move(callbacks.front());
			callbacks.pop_front();
			callback();
		}

		std::vector<std::string>                             pages{ "General" };
		bool                                                 menuBufferOnly{};
		std::optional<MCMBridge::ClassicPageSelection>       currentPage;
		mutable std::vector<MCMBridge::ClassicPageSelection> readPages;
		MCMBridge::MCMPage                                   page;
		std::vector<MCMBridge::ClassicMethod>                calls;
		std::deque<std::function<void()>>                    callbacks;
		std::optional<MCMBridge::ClassicMethod>              failedMethod;
		std::optional<MCMBridge::MCMValue>                   selectResult;
		std::optional<MCMBridge::MCMValue>                   resetResult;
		std::function<void(MCMBridge::ClassicMethod)>        onComplete;
		MCMBridge::Result<MCMBridge::SliderMetadata>         sliderMetadata{
			std::unexpected(MCMBridge::BridgeError{ MCMBridge::BridgeErrorCode::kInvalidData, "No slider" })
		};
		MCMBridge::Result<MCMBridge::MenuMetadata> menuMetadata{
			std::unexpected(MCMBridge::BridgeError{ MCMBridge::BridgeErrorCode::kInvalidData, "No menu" })
		};
		MCMBridge::Result<MCMBridge::ColorMetadata> colorMetadata{ MCMBridge::ColorMetadata{} };
		MCMBridge::Result<MCMBridge::InputMetadata> inputMetadata{ MCMBridge::InputMetadata{} };
		MCMBridge::MCMValue                         currentValue{ true };
		bool                                        configOpen{};
		bool                                        pageReady{ true };
		bool                                        missingPageBuffers{};
		bool                                        deferCallbacks{};
		bool                                        livePage{};
		std::string                                 pageTitle;
		std::string                                 infoText;
	};

	MCMBridge::MCMDescriptor Descriptor()
	{
		return {
			.stableID = "classic-mod:fixture",
			.displayName = "Fixture",
			.ownerPlugin = "Fixture.esp",
			.questFormID = 0x1234,
			.scriptName = "FixtureConfig"
		};
	}

	std::shared_ptr<FakeScript> ScriptWithToggle()
	{
		auto                  script = std::make_shared<FakeScript>();
		MCMBridge::MCMControl control;
		control.type = MCMBridge::MCMControlType::kToggle;
		control.identity.stableID = "setting:toggle";
		control.value = true;
		script->page.stableID = "page:general";
		script->page.controls.push_back(std::move(control));
		return script;
	}

	bool Called(const FakeScript& a_script, MCMBridge::ClassicMethod a_method)
	{
		return std::ranges::find(a_script.calls, a_method) != a_script.calls.end();
	}

	std::size_t CallCount(const FakeScript& a_script, MCMBridge::ClassicMethod a_method)
	{
		return static_cast<std::size_t>(std::ranges::count(a_script.calls, a_method));
	}
}

TEST_CASE("Registry and backend adapters are replaceable")
{
	FakeRegistry registry;
	registry.entries.push_back(Descriptor());
	auto entries = registry.Read();
	REQUIRE(entries);
	CHECK(entries->size() == 1);

	registry.error = MCMBridge::BridgeError{ MCMBridge::BridgeErrorCode::kInvalidData, "Missing registry property" };
	CHECK_FALSE(registry.Read());

	FakeBackend                                         backend;
	std::optional<MCMBridge::Result<MCMBridge::MCMMod>> snapshot;
	backend.BuildSnapshot(Descriptor(), [&](auto a_result) { snapshot = std::move(a_result); });
	REQUIRE(snapshot);
	REQUIRE(*snapshot);
	CHECK((*snapshot)->stableID == "classic-mod:fixture");
}

TEST_CASE("Navigation discovery reads names without visiting pages or requesting dialogs")
{
	auto script = ScriptWithToggle();
	script->pages = { "Names (1/2)", "Dynamic", "" };
	FakeTimer                                           timer;
	MCMBridge::UnavailableMenuOptionResolver            resolver;
	std::optional<MCMBridge::Result<MCMBridge::MCMMod>> result;
	auto                                                operation = std::make_shared<MCMBridge::ClassicScanOperation>(
		Descriptor(), script, resolver, [] { return false; }, [&](auto a_value) { result = std::move(a_value); }, timer);
	operation->SetNavigationOnly(true);
	operation->Start();
	REQUIRE(result);
	REQUIRE(*result);
	REQUIRE((*result)->pages.size() == 2);
	CHECK((*result)->pages[0].rawName == "Names (1/2)");
	CHECK((*result)->pages[0].controls.empty());
	CHECK(script->readPages.empty());
	REQUIRE(script->calls.size() == 2);
	CHECK(script->calls[0] == MCMBridge::ClassicMethod::kOpenConfig);
	CHECK(script->calls[1] == MCMBridge::ClassicMethod::kCloseConfig);
}

TEST_CASE("Restore scan reuses the freshly opened page only with matching valid identity")
{
	auto script = ScriptWithToggle();
	script->capturedOpeningPage = true;
	script->pages.clear();
	script->currentPage = MCMBridge::ClassicPageSelection{ "", -1 };
	bool rebuild{};
	SECTION("Fresh opening page") {}
	SECTION("Uncaptured opening page")
	{
		script->capturedOpeningPage = false;
		rebuild = true;
	}
	SECTION("Invalidated page")
	{
		script->reusablePage = false;
		rebuild = true;
	}
	SECTION("Different raw page")
	{
		script->currentPage->name = "Other";
		rebuild = true;
	}
	SECTION("Different index")
	{
		script->currentPage->index = 0;
		rebuild = true;
	}
	FakeTimer                                           timer;
	MCMBridge::UnavailableMenuOptionResolver            resolver;
	std::optional<MCMBridge::Result<MCMBridge::MCMMod>> result;
	auto                                                operation = std::make_shared<MCMBridge::ClassicScanOperation>(
		Descriptor(), script, resolver, [] { return false; }, [&](auto a_value) { result = std::move(a_value); }, timer);
	operation->SetReuseCurrentPage(true);
	operation->Start();
	REQUIRE(result);
	REQUIRE(*result);
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kOpenConfig) == 1);
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kCloseConfig) == 1);
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kSetPage) == (rebuild ? 1 : 0));
	CHECK(script->readPages.size() == 1);
}

TEST_CASE("Navigation refresh observes new pages without reading unrelated controls")
{
	auto script = ScriptWithToggle();
	script->pages = { "General", "Unlocked" };
	FakeTimer                                           timer;
	MCMBridge::UnavailableMenuOptionResolver            resolver;
	std::optional<MCMBridge::Result<MCMBridge::MCMMod>> result;
	auto                                                operation = std::make_shared<MCMBridge::ClassicScanOperation>(
		Descriptor(), script, resolver, [] { return false; }, [&](auto a_value) { result = std::move(a_value); }, timer);
	operation->SetNavigationOnly(true);
	operation->Start();
	REQUIRE(result);
	REQUIRE(*result);
	REQUIRE((*result)->pages.size() == 2);
	CHECK((*result)->pages[1].rawName == "Unlocked");
	CHECK(script->readPages.empty());
	CHECK_FALSE(Called(*script, MCMBridge::ClassicMethod::kSetPage));
	CHECK_FALSE(Called(*script, MCMBridge::ClassicMethod::kRequestSliderDialogData));
	CHECK(Called(*script, MCMBridge::ClassicMethod::kCloseConfig));
}

TEST_CASE("Navigation discovery preserves a Classic menu without named pages")
{
	auto script = ScriptWithToggle();
	script->pages.clear();
	FakeTimer                                           timer;
	MCMBridge::UnavailableMenuOptionResolver            resolver;
	std::optional<MCMBridge::Result<MCMBridge::MCMMod>> result;
	auto                                                operation = std::make_shared<MCMBridge::ClassicScanOperation>(
		Descriptor(), script, resolver, [] { return false; }, [&](auto a_value) { result = std::move(a_value); }, timer);
	operation->SetNavigationOnly(true);
	operation->Start();
	REQUIRE(result);
	REQUIRE(*result);
	REQUIRE((*result)->pages.size() == 1);
	CHECK((*result)->pages.front().rawName.empty());
	CHECK((*result)->pages.front().index == -1);
	CHECK(script->readPages.empty());
	REQUIRE(script->calls.size() == 2);
	CHECK(script->calls.back() == MCMBridge::ClassicMethod::kCloseConfig);
}

TEST_CASE("Hosted activation collects metadata only for the requested page")
{
	auto script = ScriptWithToggle();
	script->pages = { "General", "Other" };
	auto& control = script->page.controls.front();
	control.type = MCMBridge::MCMControlType::kInput;
	FakeTimer                                            timer;
	MCMBridge::UnavailableMenuOptionResolver             resolver;
	std::optional<MCMBridge::Result<MCMBridge::MCMPage>> result;
	auto                                                 operation = std::make_shared<MCMBridge::HostedPageOperation>(
		Descriptor(), script, "General", 0, false, MCMBridge::HostedPageMode::kActivate,
		[] { return false; }, [&](auto a_value) { result = std::move(a_value); }, timer);
	operation->SetMenuResolver(resolver);
	operation->Start();
	REQUIRE(result);
	REQUIRE(*result);
	REQUIRE((*result)->controls.front().input);
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kSetPage) == 1);
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kRequestInputDialogData) == 1);
	CHECK_FALSE(Called(*script, MCMBridge::ClassicMethod::kCloseConfig));
}

TEST_CASE("A host adapter supplies sessions without Skyrim types")
{
	class Adapter final : public MCMBridge::IMCMHostAdapter
	{
	public:
		std::shared_ptr<MCMBridge::IClassicScript> CreateSession() const override
		{
			return ScriptWithToggle();
		}
	};
	Adapter adapter;
	auto    session = adapter.CreateSession();
	REQUIRE(session);
	CHECK(session->ReadPages() == std::vector<std::string>{ "General" });
}

TEST_CASE("Hosted dropdowns consume native options without requiring Scaleform capture")
{
	using namespace MCMBridge;
	auto  script = ScriptWithToggle();
	auto& control = script->page.controls.front();
	control.type = MCMControlType::kMenu;
	control.writeCapability = WriteCapability::kMissingOptions;
	script->menuMetadata = MenuMetadata{ .options = { "First", "Second" }, .selectedIndex = 1, .defaultIndex = 0, .availability = MetadataAvailability::kAvailable };
	ScopedMenuOptionResolver resolver;
	bool                     missing{};
	SECTION("Native options are sufficient") {}
	SECTION("Legacy buffer-only options are still captured")
	{
		script->menuBufferOnly = true;
		script->onComplete = [&resolver](auto a_method) {
			if (a_method == ClassicMethod::kRequestMenuDialogData)
				resolver.ObserveInvokeStringArray("Journal Menu", "_root.ConfigPanelFader.configPanel.setMenuDialogOptions", { "First", "Second" });
		};
	}
	SECTION("Missing options stay read-only")
	{
		script->menuBufferOnly = true;
		missing = true;
	}
	SECTION("Disabled native menus stay disabled") { control.disabled = true; }
	FakeTimer                      timer;
	std::optional<Result<MCMPage>> result;
	auto                           operation = std::make_shared<HostedPageOperation>(Descriptor(), script, "General", 0, false, HostedPageMode::kActivate, [] { return false; }, [&](auto a_value) { result = std::move(a_value); }, timer);
	operation->SetMenuResolver(resolver);
	operation->Start();
	REQUIRE(result);
	REQUIRE(*result);
	const auto& actual = (*result)->controls.front();
	if (missing) {
		CHECK_FALSE(actual.menu);
		CHECK(actual.writeCapability == WriteCapability::kMissingOptions);
	} else {
		REQUIRE(actual.menu);
		CHECK(actual.menu->options == std::vector<std::string>{ "First", "Second" });
		CHECK(std::get<std::int32_t>(actual.value) == 1);
		CHECK(actual.writeCapability == (control.disabled ? WriteCapability::kDisabled : WriteCapability::kWritable));
	}
	CHECK(CallCount(*script, ClassicMethod::kRequestMenuDialogData) == 1);
	CHECK_FALSE(resolver.Resolve(control.identity));
}

TEST_CASE("Classic scan succeeds and closes the config")
{
	auto                                                script = ScriptWithToggle();
	FakeTimer                                           timer;
	MCMBridge::UnavailableMenuOptionResolver            resolver;
	std::optional<MCMBridge::Result<MCMBridge::MCMMod>> result;
	auto                                                operation = std::make_shared<MCMBridge::ClassicScanOperation>(
		Descriptor(), script, resolver, [] { return false; }, [&](auto a_value) { result = std::move(a_value); }, timer);
	operation->Start();

	REQUIRE(result);
	REQUIRE(*result);
	CHECK((*result)->pages.size() == 1);
	CHECK(Called(*script, MCMBridge::ClassicMethod::kCloseConfig));
	CHECK_FALSE(script->configOpen);
}

TEST_CASE("Slider dialog metadata preserves the current page value")
{
	using namespace MCMBridge;
	auto script = ScriptWithToggle();
	script->page.controls.front().type = MCMControlType::kSlider;
	script->page.controls.front().value = 4.0F;
	script->sliderMetadata = SliderMetadata{ .start = 7.0F, .defaultValue = 2.0F, .minimum = 0.0F, .maximum = 10.0F, .step = 1.0F };
	FakeTimer                     timer;
	UnavailableMenuOptionResolver resolver;
	std::optional<Result<MCMMod>> result;
	auto                          operation = std::make_shared<ClassicScanOperation>(Descriptor(), script, resolver, [] { return false; }, [&](auto a_result) { result = std::move(a_result); }, timer);
	operation->Start();
	REQUIRE(result);
	REQUIRE(*result);
	CHECK(std::ranges::count(script->calls, ClassicMethod::kRequestSliderDialogData) == 1);
	CHECK(std::get<float>((*result)->pages.front().controls.front().value) == 4.0F);
	CHECK((*result)->pages.front().controls.front().slider->start == 7.0F);
	CHECK(Called(*script, ClassicMethod::kCloseConfig));
}

TEST_CASE("NL MCM scan preserves the opening page and page scoped identity")
{
	auto script = ScriptWithToggle();
	script->pages = { "First", "Second" };
	script->currentPage = MCMBridge::ClassicPageSelection{ "Second", 1 };
	auto descriptor = Descriptor();
	descriptor.pageScopedState = true;
	FakeTimer                                           timer;
	MCMBridge::UnavailableMenuOptionResolver            resolver;
	std::optional<MCMBridge::Result<MCMBridge::MCMMod>> result;
	auto                                                operation = std::make_shared<MCMBridge::ClassicScanOperation>(
		descriptor, script, resolver, [] { return false; }, [&](auto a_value) { result = std::move(a_value); }, timer);
	operation->Start();

	REQUIRE(result);
	REQUIRE(*result);
	CHECK((*result)->pageScopedState);
	REQUIRE(script->readPages.size() == 2);
	CHECK(script->readPages[0].name == "Second");
	CHECK(script->readPages[0].index == 1);
	CHECK(script->readPages[1].name == "First");
	CHECK(script->readPages[1].index == 0);
}

TEST_CASE("Classic scans never dispatch navigation placeholders to Papyrus")
{
	auto script = ScriptWithToggle();
	script->pages = { "", "First", " ", "None", "Second", "" };
	FakeTimer                                           timer;
	MCMBridge::UnavailableMenuOptionResolver            resolver;
	std::optional<MCMBridge::Result<MCMBridge::MCMMod>> result;
	auto                                                operation = std::make_shared<MCMBridge::ClassicScanOperation>(
		Descriptor(), script, resolver, [] { return false; }, [&](auto a_value) { result = std::move(a_value); }, timer);
	operation->Start();

	REQUIRE(result);
	REQUIRE(*result);
	CHECK((*result)->pages.size() == 2);
	REQUIRE(script->readPages.size() == 2);
	CHECK(script->readPages[0] == MCMBridge::ClassicPageSelection{ "First", 1 });
	CHECK(script->readPages[1] == MCMBridge::ClassicPageSelection{ "Second", 4 });
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kSetPage) == 2);
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kCloseConfig) == 1);
	CHECK_FALSE(Called(*script, MCMBridge::ClassicMethod::kSelectOption));
	CHECK_FALSE(script->configOpen);
}

TEST_CASE("Classic scans preserve unnamed single-page settings")
{
	auto script = ScriptWithToggle();
	script->pages = { "", "None", " " };
	FakeTimer                                           timer;
	MCMBridge::UnavailableMenuOptionResolver            resolver;
	std::optional<MCMBridge::Result<MCMBridge::MCMMod>> result;
	auto                                                operation = std::make_shared<MCMBridge::ClassicScanOperation>(
		Descriptor(), script, resolver, [] { return false; }, [&](auto a_value) { result = std::move(a_value); }, timer);
	operation->Start();

	REQUIRE(result);
	REQUIRE(*result);
	REQUIRE((*result)->pages.size() == 1);
	CHECK_FALSE((*result)->pages.front().controls.empty());
	CHECK(script->readPages == std::vector<MCMBridge::ClassicPageSelection>{ { "", -1 } });
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kCloseConfig) == 1);
}

TEST_CASE("Classic scan closes after recoverable failures and yields to Journal ownership")
{
	MCMBridge::UnavailableMenuOptionResolver resolver;
	FakeTimer                                timer;

	SECTION("Missing page buffers")
	{
		auto script = ScriptWithToggle();
		script->missingPageBuffers = true;
		std::optional<MCMBridge::Result<MCMBridge::MCMMod>> result;
		auto                                                operation = std::make_shared<MCMBridge::ClassicScanOperation>(
			Descriptor(), script, resolver, [] { return false; }, [&](auto a_value) { result = std::move(a_value); }, timer);
		operation->Start();
		REQUIRE(result);
		CHECK_FALSE(*result);
		CHECK(Called(*script, MCMBridge::ClassicMethod::kCloseConfig));
	}

	SECTION("Callback dispatch failure")
	{
		auto script = ScriptWithToggle();
		script->failedMethod = MCMBridge::ClassicMethod::kSetPage;
		std::optional<MCMBridge::Result<MCMBridge::MCMMod>> result;
		auto                                                operation = std::make_shared<MCMBridge::ClassicScanOperation>(
			Descriptor(), script, resolver, [] { return false; }, [&](auto a_value) { result = std::move(a_value); }, timer);
		operation->Start();
		REQUIRE(result);
		CHECK_FALSE(*result);
		CHECK(Called(*script, MCMBridge::ClassicMethod::kCloseConfig));
	}

	SECTION("Journal interruption")
	{
		auto                                                script = ScriptWithToggle();
		std::size_t                                         checks = 0;
		std::optional<MCMBridge::Result<MCMBridge::MCMMod>> result;
		auto                                                operation = std::make_shared<MCMBridge::ClassicScanOperation>(
			Descriptor(), script, resolver, [&] { return ++checks >= 3; }, [&](auto a_value) { result = std::move(a_value); }, timer);
		operation->Start();
		REQUIRE(result);
		REQUIRE_FALSE(*result);
		CHECK((*result).error().code == MCMBridge::BridgeErrorCode::kBusy);
		CHECK_FALSE(Called(*script, MCMBridge::ClassicMethod::kCloseConfig));
		CHECK(script->configOpen);
	}
}

TEST_CASE("Classic scan does not continue a queued callback after Journal ownership changes")
{
	auto script = ScriptWithToggle();
	script->deferCallbacks = true;
	bool                                                journalOpen{};
	FakeTimer                                           timer;
	MCMBridge::UnavailableMenuOptionResolver            resolver;
	std::optional<MCMBridge::Result<MCMBridge::MCMMod>> result;
	auto                                                operation = std::make_shared<MCMBridge::ClassicScanOperation>(
		Descriptor(), script, resolver, [&] { return journalOpen; },
		[&](auto a_value) { result = std::move(a_value); }, timer);
	operation->Start();
	REQUIRE(script->calls == std::vector{ MCMBridge::ClassicMethod::kOpenConfig });

	journalOpen = true;
	script->CompleteNext();

	REQUIRE(result);
	REQUIRE_FALSE(*result);
	CHECK(result->error().code == MCMBridge::BridgeErrorCode::kBusy);
	CHECK(script->calls == std::vector{ MCMBridge::ClassicMethod::kOpenConfig });
	CHECK_FALSE(Called(*script, MCMBridge::ClassicMethod::kCloseConfig));
}

TEST_CASE("Classic scan ignores callbacks after timeout and save invalidation")
{
	MCMBridge::UnavailableMenuOptionResolver resolver;
	FakeTimer                                timer;

	SECTION("Timeout and late callback")
	{
		auto script = ScriptWithToggle();
		script->deferCallbacks = true;
		std::size_t                completions = 0;
		MCMBridge::BridgeErrorCode error{};
		auto                       operation = std::make_shared<MCMBridge::ClassicScanOperation>(
			Descriptor(), script, resolver, [] { return false; }, [&](auto a_result) {
				++completions;
				if (!a_result) {
					error = a_result.error().code;
				} }, timer);
		operation->Start();
		REQUIRE(timer.tasks.size() == 1);
		timer.RunNext();
		CHECK(completions == 1);
		CHECK(error == MCMBridge::BridgeErrorCode::kTimedOut);
		script->CompleteNext();
		CHECK(completions == 1);
		CHECK_FALSE(Called(*script, MCMBridge::ClassicMethod::kCloseConfig));
	}

	SECTION("Save invalidation")
	{
		auto script = ScriptWithToggle();
		script->deferCallbacks = true;
		std::size_t completions = 0;
		auto        operation = std::make_shared<MCMBridge::ClassicScanOperation>(
			Descriptor(), script, resolver, [] { return false; }, [&](auto) { ++completions; }, timer);
		operation->Start();
		operation->Cancel();
		CHECK(completions == 1);
		script->CompleteNext();
		CHECK(completions == 1);
	}
}

TEST_CASE("Classic toggle write executes its callback once and confirms the value")
{
	auto script = ScriptWithToggle();
	auto control = script->page.controls.front();
	control.identity.pageKey = "General";
	control.identity.pageIndex = 0;
	control.writeCapability = MCMBridge::WriteCapability::kWritable;
	MCMBridge::WriteCommand command{
		.snapshotGeneration = 1,
		.settingID = control.identity.stableID,
		.expectedIdentity = control.identity,
		.expectedType = control.type,
		.expectedValue = true,
		.desiredValue = false
	};
	FakeTimer                                             timer;
	std::optional<MCMBridge::Result<MCMBridge::MCMValue>> result;
	auto                                                  operation = std::make_shared<MCMBridge::ClassicWriteOperation>(
		script, control, command, [] { return false; }, [&](auto a_value) { result = std::move(a_value); }, timer);
	operation->Start();

	REQUIRE(result);
	REQUIRE(*result);
	CHECK(std::get<bool>(**result) == false);
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kSelectOption) == 1);
	CHECK(Called(*script, MCMBridge::ClassicMethod::kCloseConfig));
}

TEST_CASE("Hosted write survives message wait and executes the callback only once")
{
	auto script = ScriptWithToggle();
	script->configOpen = true;
	script->deferCallbacks = true;
	auto                    control = script->page.controls.front();
	MCMBridge::WriteCommand command{
		.expectedIdentity = control.identity,
		.expectedType = control.type,
		.expectedValue = true,
		.desiredValue = false
	};
	FakeTimer                                             timer;
	std::optional<MCMBridge::Result<MCMBridge::MCMValue>> result;
	auto                                                  operation = std::make_shared<MCMBridge::ClassicWriteOperation>(script, control, command, [] { return false; }, [&](auto a_result) { result = std::move(a_result); }, timer, MCMBridge::ClassicWriteMode::kHosted);
	operation->Start();
	REQUIRE(CallCount(*script, MCMBridge::ClassicMethod::kSelectOption) == 1);
	script->messageWait = std::chrono::seconds(120);
	timer.RunNext();
	CHECK_FALSE(result);
	CHECK_FALSE(script->retired);
	script->deferCallbacks = false;
	script->CompleteNext();
	REQUIRE(result);
	REQUIRE(*result);
	CHECK(std::get<bool>(**result) == false);
	timer.RunNext();
	CHECK_FALSE(script->retired);
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kSelectOption) == 1);
}

TEST_CASE("Direct host message wait preserves one completion and cancellation")
{
	auto script = ScriptWithToggle();
	script->deferCallbacks = true;
	FakeTimer                              timer;
	auto                                   session = std::make_shared<MCMBridge::HostCallSession>(script, timer);
	std::vector<MCMBridge::HostCallStatus> results;
	REQUIRE(session->Submit({ MCMBridge::ClassicMethod::kSelectOption }, [&](auto a_status) { results.push_back(a_status); }, std::chrono::seconds(10)));
	script->messageWait = std::chrono::seconds(90);
	timer.RunNext();
	CHECK(results.empty());
	CHECK_FALSE(script->retired);
	SECTION("Completion")
	{
		script->CompleteNext();
		REQUIRE(results == std::vector{ MCMBridge::HostCallStatus::kCompleted });
	}
	SECTION("Save invalidation")
	{
		session->Invalidate();
		script->CompleteNext();
		REQUIRE(results == std::vector{ MCMBridge::HostCallStatus::kSessionInvalidated });
	}
	SECTION("Execution hangs after dialog completion")
	{
		timer.RunNext();
		REQUIRE(results == std::vector{ MCMBridge::HostCallStatus::kTimedOut });
		CHECK(script->retired);
		script->CompleteNext();
	}
	while (!timer.tasks.empty()) timer.RunNext();
	CHECK(results.size() == 1);
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kSelectOption) == 1);
}

TEST_CASE("Classic write yields without cleanup when the Journal takes ownership")
{
	auto script = ScriptWithToggle();
	script->deferCallbacks = true;
	auto control = script->page.controls.front();
	control.identity.pageKey = "General";
	control.identity.pageIndex = 0;
	control.writeCapability = MCMBridge::WriteCapability::kWritable;
	MCMBridge::WriteCommand command{
		.snapshotGeneration = 1,
		.settingID = control.identity.stableID,
		.expectedIdentity = control.identity,
		.expectedType = control.type,
		.expectedValue = true,
		.desiredValue = false
	};
	FakeTimer                                             timer;
	std::optional<MCMBridge::Result<MCMBridge::MCMValue>> result;
	auto                                                  operation = std::make_shared<MCMBridge::ClassicWriteOperation>(
		script, control, command, [] { return false; }, [&](auto a_value) { result = std::move(a_value); }, timer);
	operation->Start();
	REQUIRE(script->calls == std::vector{ MCMBridge::ClassicMethod::kOpenConfig });

	operation->YieldToFrontend();

	REQUIRE(result);
	REQUIRE_FALSE(*result);
	CHECK(result->error().code == MCMBridge::BridgeErrorCode::kBusy);
	script->CompleteNext();
	CHECK(script->calls == std::vector{ MCMBridge::ClassicMethod::kOpenConfig });
	CHECK_FALSE(Called(*script, MCMBridge::ClassicMethod::kCloseConfig));
}

TEST_CASE("Menu write validates the live list before writing")
{
	auto script = ScriptWithToggle();
	auto control = script->page.controls.front();
	control.type = MCMBridge::MCMControlType::kMenu;
	control.identity.pageKey = "General";
	control.writeCapability = MCMBridge::WriteCapability::kWritable;
	control.menu = MCMBridge::MenuMetadata{
		.options = { "$First", "$Second" },
		.selectedIndex = 0,
		.availability = MCMBridge::MetadataAvailability::kAvailable
	};
	script->page.controls.front() = control;
	script->menuMetadata = *control.menu;
	script->currentValue = std::int32_t{ 0 };
	MCMBridge::WriteCommand command{
		.settingID = control.identity.stableID,
		.expectedIdentity = control.identity,
		.expectedType = control.type,
		.expectedValue = std::int32_t{ 0 },
		.desiredValue = std::int32_t{ 1 }
	};
	bool changed{};
	SECTION("Unchanged list") {}
	SECTION("Reordered list")
	{
		script->menuMetadata->options = { "$Second", "$First" };
		changed = true;
	}
	FakeTimer                                             timer;
	std::optional<MCMBridge::Result<MCMBridge::MCMValue>> result;
	auto                                                  operation = std::make_shared<MCMBridge::ClassicWriteOperation>(
		script, control, command, [] { return false; }, [&](auto a_value) { result = std::move(a_value); }, timer);
	MCMBridge::UnavailableMenuOptionResolver resolver;
	operation->SetMenuResolver(resolver);
	operation->Start();
	REQUIRE(result);
	if (changed) {
		REQUIRE_FALSE(*result);
		CHECK(result->error().code == MCMBridge::BridgeErrorCode::kStaleSnapshot);
		CHECK_FALSE(Called(*script, MCMBridge::ClassicMethod::kSetMenuIndex));
	} else {
		REQUIRE(*result);
		CHECK(std::get<std::int32_t>(**result) == 1);
		CHECK(CallCount(*script, MCMBridge::ClassicMethod::kSetMenuIndex) == 1);
	}
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kCloseConfig) == 1);
}

TEST_CASE("Hosted toggle write reuses the open page and leaves the config open")
{
	auto script = ScriptWithToggle();
	script->configOpen = true;
	auto control = script->page.controls.front();
	control.identity.pageKey = "General";
	control.identity.pageIndex = 0;
	control.writeCapability = MCMBridge::WriteCapability::kWritable;
	MCMBridge::WriteCommand command{
		.snapshotGeneration = 1,
		.settingID = control.identity.stableID,
		.expectedIdentity = control.identity,
		.expectedType = control.type,
		.expectedValue = true,
		.desiredValue = false
	};
	FakeTimer                                             timer;
	std::optional<MCMBridge::Result<MCMBridge::MCMValue>> result;
	auto                                                  operation = std::make_shared<MCMBridge::ClassicWriteOperation>(
		script,
		control,
		command,
		[] { return false; },
		[&](auto a_value) { result = std::move(a_value); },
		timer,
		MCMBridge::ClassicWriteMode::kHosted);
	operation->Start();

	REQUIRE(result);
	REQUIRE(*result);
	CHECK_FALSE(std::get<bool>(**result));
	CHECK_FALSE(Called(*script, MCMBridge::ClassicMethod::kOpenConfig));
	CHECK_FALSE(Called(*script, MCMBridge::ClassicMethod::kCloseConfig));
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kSetPage) == 1);
	CHECK(script->configOpen);
}

TEST_CASE("A write retains script navigation while confirming the original setting exactly once")
{
	auto script = ScriptWithToggle();
	script->configOpen = true;
	auto control = script->page.controls.front();
	control.identity.pageKey = "General";
	control.identity.pageIndex = 0;
	script->onComplete = [script](auto a_method) {
		if (a_method == MCMBridge::ClassicMethod::kSelectOption)
			script->pageRedirect = MCMBridge::ClassicPageSelection{ "Advanced", 1 };
	};
	MCMBridge::WriteCommand                               command{ .settingID = control.identity.stableID, .expectedIdentity = control.identity, .expectedType = control.type, .expectedValue = true, .desiredValue = false };
	FakeTimer                                             timer;
	std::optional<MCMBridge::Result<MCMBridge::MCMValue>> result;
	auto                                                  operation = std::make_shared<MCMBridge::ClassicWriteOperation>(script, control, command, [] { return false; }, [&](auto a_result) { result = std::move(a_result); }, timer, MCMBridge::ClassicWriteMode::kHosted);
	operation->Start();
	REQUIRE(result);
	REQUIRE(*result);
	REQUIRE(operation->PageRedirect());
	CHECK(operation->PageRedirect()->name == "Advanced");
	CHECK(operation->PageRedirect()->index == 1);
	CHECK_FALSE(script->pageRedirect);
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kSelectOption) == 1);
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kSetPage) == 1);
	script->onComplete = {};
}

TEST_CASE("Hosted write timeout does not start a competing close")
{
	MCMBridge::WritePauseState pause;
	const auto                 first = pause.Reserve();
	const auto                 second = pause.Reserve();
	REQUIRE(pause.Begin(first));
	auto script = ScriptWithToggle();
	script->configOpen = true;
	script->deferCallbacks = true;
	auto control = script->page.controls.front();
	control.identity.pageKey = "General";
	control.identity.pageIndex = 0;
	control.writeCapability = MCMBridge::WriteCapability::kWritable;
	MCMBridge::WriteCommand command{
		.snapshotGeneration = 1,
		.settingID = control.identity.stableID,
		.expectedIdentity = control.identity,
		.expectedType = control.type,
		.expectedValue = true,
		.desiredValue = false
	};
	FakeTimer                  timer;
	std::size_t                completions{};
	MCMBridge::BridgeErrorCode error{};
	auto                       operation = std::make_shared<MCMBridge::ClassicWriteOperation>(
		script,
		control,
		command,
		[] { return false; },
		[&](auto a_result) {
			++completions;
			CHECK(pause.Complete(first));
			if (!a_result) {
				error = a_result.error().code;
			}
		},
		timer,
		MCMBridge::ClassicWriteMode::kHosted);
	operation->Start();
	REQUIRE(timer.tasks.size() == 1);
	timer.RunNext();
	CHECK(completions == 1);
	CHECK(error == MCMBridge::BridgeErrorCode::kTimedOut);
	CHECK_FALSE(Called(*script, MCMBridge::ClassicMethod::kCloseConfig));
	CHECK(pause.ShouldPause());
	CHECK(pause.Pending() == 1);

	script->CompleteNext();
	CHECK(completions == 1);
	CHECK_FALSE(Called(*script, MCMBridge::ClassicMethod::kCloseConfig));
	CHECK(pause.ShouldPause());
	REQUIRE(pause.Complete(second));
	CHECK_FALSE(pause.ShouldPause());
}

TEST_CASE("Hosted current-page adoption never dispatches a second page callback")
{
	auto script = ScriptWithToggle();
	script->configOpen = true;
	script->currentPage = MCMBridge::ClassicPageSelection{ "General", 0 };
	FakeTimer                                            timer;
	std::optional<MCMBridge::Result<MCMBridge::MCMPage>> result;
	SECTION("Completed page is adopted") {}
	SECTION("Closed config is rejected") { script->configOpen = false; }
	SECTION("Different raw page is rejected") { script->currentPage->name = "Advanced"; }
	SECTION("Different page index is rejected") { script->currentPage->index = 1; }
	SECTION("Unknown current page is rejected") { script->currentPage.reset(); }
	const auto valid = script->configOpen && script->currentPage == MCMBridge::ClassicPageSelection{ "General", 0 };
	int        completions{};
	auto       operation = std::make_shared<MCMBridge::HostedPageOperation>(
		Descriptor(), script, "General", 0, true, MCMBridge::HostedPageMode::kReadCurrent,
		[] { return false; }, [&](auto a_result) { ++completions; result = std::move(a_result); }, timer);
	operation->Start();
	REQUIRE(result);
	CHECK(result->has_value() == valid);
	CHECK(script->calls.empty());
	CHECK(timer.tasks.empty());
	operation->Cancel();
	CHECK(completions == 1);
}

TEST_CASE("Borrowed page metadata detects redirects without replaying the page callback")
{
	auto script = ScriptWithToggle();
	script->configOpen = true;
	script->currentPage = MCMBridge::ClassicPageSelection{ "General", 0 };
	script->page.controls.front().type = MCMBridge::MCMControlType::kSlider;
	script->sliderMetadata = MCMBridge::SliderMetadata{};
	script->deferCallbacks = true;
	FakeTimer                                            timer;
	MCMBridge::UnavailableMenuOptionResolver             resolver;
	std::optional<MCMBridge::Result<MCMBridge::MCMPage>> result;
	auto                                                 operation = std::make_shared<MCMBridge::HostedPageOperation>(
		Descriptor(), script, "General", 0, true, MCMBridge::HostedPageMode::kReadCurrent,
		[] { return false; }, [&](auto a_result) { result = std::move(a_result); }, timer);
	operation->SetMenuResolver(resolver);
	operation->Start();
	REQUIRE_FALSE(result);
	REQUIRE(script->calls == std::vector{ MCMBridge::ClassicMethod::kRequestSliderDialogData });
	script->currentPage = MCMBridge::ClassicPageSelection{ "Advanced", 1 };
	script->CompleteNext();
	REQUIRE(result);
	REQUIRE_FALSE(*result);
	CHECK(result->error().code == MCMBridge::BridgeErrorCode::kStaleSnapshot);
	CHECK(script->calls.size() == 1);
}

TEST_CASE("Borrowed unnamed opening page remains readable beside named modules")
{
	auto script = ScriptWithToggle();
	script->configOpen = true;
	script->livePage = true;
	script->pages = { "First", "Second" };
	script->currentPage = MCMBridge::ClassicPageSelection{ "", -1 };
	FakeTimer                                            timer;
	std::optional<MCMBridge::Result<MCMBridge::MCMPage>> result;
	auto                                                 operation = std::make_shared<MCMBridge::HostedPageOperation>(
		Descriptor(), script, "", -1, true, MCMBridge::HostedPageMode::kReadCurrent,
		[] { return false; }, [&](auto a_result) { result = std::move(a_result); }, timer);
	operation->SetExpectedPages({ { "", -1 }, { "First", 0 }, { "Second", 1 } });
	operation->Start();
	REQUIRE(result);
	REQUIRE(*result);
	CHECK((**result).rawName.empty());
	CHECK((**result).index == -1);
	CHECK((**result).controls.size() == 1);
	CHECK(script->calls.empty());
}

TEST_CASE("Hosted page activation opens once and reuses the config")
{
	auto                                                 script = ScriptWithToggle();
	FakeTimer                                            timer;
	std::optional<MCMBridge::Result<MCMBridge::MCMPage>> first;
	auto                                                 open = std::make_shared<MCMBridge::HostedPageOperation>(
		Descriptor(),
		script,
		"General",
		0,
		false,
		MCMBridge::HostedPageMode::kActivate,
		[] { return false; },
		[&](auto a_result) { first = std::move(a_result); },
		timer);
	open->Start();

	REQUIRE(first);
	REQUIRE(*first);
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kOpenConfig) == 1);
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kSetPage) == 1);
	CHECK(open->IsConfigOpen());

	std::optional<MCMBridge::Result<MCMBridge::MCMPage>> second;
	auto                                                 reuse = std::make_shared<MCMBridge::HostedPageOperation>(
		Descriptor(),
		script,
		"General",
		0,
		true,
		MCMBridge::HostedPageMode::kActivate,
		[] { return false; },
		[&](auto a_result) { second = std::move(a_result); },
		timer);
	reuse->Start();

	REQUIRE(second);
	REQUIRE(*second);
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kOpenConfig) == 1);
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kSetPage) == 2);
}

TEST_CASE("Hosted activation adopts an explicit script redirect without rebuilding its original page")
{
	auto script = ScriptWithToggle();
	script->pages = { "General", "Advanced" };
	script->livePage = true;
	script->pageRedirect = MCMBridge::ClassicPageSelection{ "Advanced", 1 };
	FakeTimer                                            timer;
	std::optional<MCMBridge::Result<MCMBridge::MCMPage>> result;
	auto                                                 operation = std::make_shared<MCMBridge::HostedPageOperation>(Descriptor(), script, "General", 0, false, MCMBridge::HostedPageMode::kActivate, [] { return false; }, [&](auto a_result) { result = std::move(a_result); }, timer);
	operation->SetExpectedPages({ { "General", 0 }, { "Advanced", 1 } });
	operation->Start();
	REQUIRE(result);
	REQUIRE(*result);
	CHECK((**result).rawName == "Advanced");
	CHECK((**result).index == 1);
	CHECK_FALSE(script->pageRedirect);
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kSetPage) == 1);
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kCloseConfig) == 0);
}

TEST_CASE("Hosted script redirects cannot bypass current navigation validation")
{
	auto script = ScriptWithToggle();
	script->pages = { "General" };
	script->pageRedirect = MCMBridge::ClassicPageSelection{ "Missing", 1 };
	FakeTimer                                            timer;
	std::optional<MCMBridge::Result<MCMBridge::MCMPage>> result;
	auto                                                 operation = std::make_shared<MCMBridge::HostedPageOperation>(Descriptor(), script, "General", 0, false, MCMBridge::HostedPageMode::kActivate, [] { return false; }, [&](auto a_result) { result = std::move(a_result); }, timer);
	operation->Start();
	REQUIRE(result);
	REQUIRE_FALSE(*result);
	CHECK(result->error().code == MCMBridge::BridgeErrorCode::kStaleSnapshot);
	CHECK(script->readPages.empty());
}

TEST_CASE("Hosted navigation rejects changed page lists before publishing empty content")
{
	auto script = ScriptWithToggle();
	script->deferCallbacks = true;
	FakeTimer                                            timer;
	std::optional<MCMBridge::Result<MCMBridge::MCMPage>> result;
	auto                                                 operation = std::make_shared<MCMBridge::HostedPageOperation>(
		Descriptor(), script, "General", 0, false, MCMBridge::HostedPageMode::kActivate,
		[] { return false; }, [&](auto a_value) { result = std::move(a_value); }, timer);
	operation->SetExpectedPages({ { "General", 0 } });
	operation->Start();
	SECTION("A renamed target is rejected after OpenConfig")
	{
		script->pages = { "General (1/2)", "General (2/2)" };
		script->CompleteNext();
		CHECK_FALSE(Called(*script, MCMBridge::ClassicMethod::kSetPage));
	}
	SECTION("A shifted index is rejected even if the name still exists")
	{
		script->pages = { "New page", "General" };
		script->CompleteNext();
		CHECK_FALSE(Called(*script, MCMBridge::ClassicMethod::kSetPage));
	}
	SECTION("An unrelated added page also invalidates navigation")
	{
		script->pages.push_back("Additional page");
		script->CompleteNext();
		CHECK_FALSE(Called(*script, MCMBridge::ClassicMethod::kSetPage));
	}
	SECTION("A change during SetPage cannot replace the snapshot")
	{
		script->CompleteNext();
		REQUIRE(Called(*script, MCMBridge::ClassicMethod::kSetPage));
		script->pages = { "General (1/2)", "General (2/2)" };
		script->page.controls.clear();
		script->CompleteNext();
		CHECK(script->readPages.empty());
	}
	REQUIRE(result);
	REQUIRE_FALSE(*result);
	CHECK(result->error().code == MCMBridge::BridgeErrorCode::kStaleSnapshot);
	CHECK_FALSE(Called(*script, MCMBridge::ClassicMethod::kCloseConfig));
}

TEST_CASE("Hosted page yields without advancing when the Journal takes ownership")
{
	auto script = ScriptWithToggle();
	script->deferCallbacks = true;
	FakeTimer                                            timer;
	std::optional<MCMBridge::Result<MCMBridge::MCMPage>> result;
	auto                                                 operation = std::make_shared<MCMBridge::HostedPageOperation>(
		Descriptor(),
		script,
		"General",
		0,
		false,
		MCMBridge::HostedPageMode::kActivate,
		[] { return false; },
		[&](auto a_value) { result = std::move(a_value); },
		timer);
	operation->Start();
	REQUIRE(script->calls == std::vector{ MCMBridge::ClassicMethod::kOpenConfig });

	operation->YieldToFrontend();

	REQUIRE(result);
	REQUIRE_FALSE(*result);
	CHECK(result->error().code == MCMBridge::BridgeErrorCode::kBusy);
	script->CompleteNext();
	CHECK(script->calls == std::vector{ MCMBridge::ClassicMethod::kOpenConfig });
	CHECK_FALSE(Called(*script, MCMBridge::ClassicMethod::kCloseConfig));
}

TEST_CASE("Hosted write commit closes and reopens the config before rebuilding the page")
{
	MCMBridge::WritePauseState pause;
	const auto                 ticket = pause.Reserve();
	REQUIRE(pause.Begin(ticket));
	auto script = ScriptWithToggle();
	script->configOpen = true;
	script->deferCallbacks = true;
	FakeTimer                                            timer;
	std::optional<MCMBridge::Result<MCMBridge::MCMPage>> result;
	auto                                                 commit = std::make_shared<MCMBridge::HostedPageOperation>(
		Descriptor(),
		script,
		"General",
		0,
		true,
		MCMBridge::HostedPageMode::kCommit,
		[] { return false; },
		[&](auto a_value) {
			result = std::move(a_value);
			CHECK(pause.Complete(ticket));
		},
		timer);
	commit->Start();
	CHECK(pause.ShouldPause());
	script->CompleteNext();
	CHECK(pause.ShouldPause());
	script->CompleteNext();
	CHECK(pause.ShouldPause());
	script->CompleteNext();
	CHECK_FALSE(pause.ShouldPause());

	REQUIRE(result);
	REQUIRE(*result);
	REQUIRE(script->calls.size() == 3);
	CHECK(script->calls[0] == MCMBridge::ClassicMethod::kCloseConfig);
	CHECK(script->calls[1] == MCMBridge::ClassicMethod::kOpenConfig);
	CHECK(script->calls[2] == MCMBridge::ClassicMethod::kSetPage);
	CHECK(script->configOpen);
	CHECK(commit->IsConfigOpen());
}

TEST_CASE("Hosted write commit republishes current dialog values and slider bounds")
{
	auto script = ScriptWithToggle();
	script->configOpen = true;
	script->deferCallbacks = true;
	auto& slider = script->page.controls.front();
	slider.type = MCMBridge::MCMControlType::kSlider;
	slider.identity.optionIndex = 0;
	slider.value = 0.0F;
	slider.slider = MCMBridge::SliderMetadata{ .format = "{1}", .availability = MCMBridge::MetadataAvailability::kMissing };
	script->sliderMetadata = MCMBridge::SliderMetadata{ .start = 7, .defaultValue = 2, .maximum = 10, .step = 0.5F, .availability = MCMBridge::MetadataAvailability::kAvailable };
	auto menu = slider;
	menu.identity.stableID = "setting:menu";
	menu.identity.optionIndex = 1;
	menu.type = MCMBridge::MCMControlType::kMenu;
	menu.slider.reset();
	menu.value = std::string("Second");
	script->page.controls.push_back(menu);
	script->menuMetadata = MCMBridge::MenuMetadata{ .options = { "First", "Second" }, .selectedIndex = 1, .defaultIndex = 0, .availability = MCMBridge::MetadataAvailability::kAvailable };
	script->onComplete = [&](auto a_method) {
		if (a_method == MCMBridge::ClassicMethod::kCloseConfig)
			script->sliderMetadata->maximum = 25;
	};
	FakeTimer                                            timer;
	std::optional<MCMBridge::Result<MCMBridge::MCMPage>> result;
	auto                                                 commit = std::make_shared<MCMBridge::HostedPageOperation>(
		Descriptor(), script, "General", 0, true, MCMBridge::HostedPageMode::kCommit,
		[] { return false; }, [&](auto a_value) { result = std::move(a_value); }, timer);
	MCMBridge::UnavailableMenuOptionResolver resolver;
	commit->SetMenuResolver(resolver);
	commit->Start();
	for (int index = 0; index < 3; ++index)
		script->CompleteNext();
	CHECK_FALSE(result);
	while (!script->callbacks.empty())
		script->CompleteNext();
	REQUIRE(result);
	REQUIRE(*result);
	const auto& controls = (*result)->controls;
	REQUIRE(controls.front().slider);
	CHECK(controls.front().slider->availability == MCMBridge::MetadataAvailability::kAvailable);
	CHECK(controls.front().slider->maximum == 25);
	CHECK(controls.front().slider->step == 0.5F);
	CHECK(controls.front().slider->format == "{1}");
	CHECK(std::get<float>(controls.front().value) == 7);
	REQUIRE(controls[1].menu);
	CHECK(controls[1].menu->options == std::vector<std::string>{ "First", "Second" });
	CHECK(std::get<std::int32_t>(controls[1].value) == 1);
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kRequestSliderDialogData) == 1);
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kRequestMenuDialogData) == 1);
	CHECK_FALSE(Called(*script, MCMBridge::ClassicMethod::kSetSliderValue));
}

TEST_CASE("Hosted close ends the config once")
{
	auto script = ScriptWithToggle();
	script->configOpen = true;
	FakeTimer                              timer;
	std::optional<MCMBridge::Result<void>> result;
	auto                                   close = std::make_shared<MCMBridge::HostedCloseOperation>(
		script,
		[&](auto a_value) { result = std::move(a_value); },
		timer);
	close->Start();

	REQUIRE(result);
	REQUIRE(*result);
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kCloseConfig) == 1);
	CHECK_FALSE(script->configOpen);
}

TEST_CASE("Hosted page activation ignores a late callback after timeout")
{
	auto script = ScriptWithToggle();
	script->deferCallbacks = true;
	MCMBridge::NativeHostSession host;
	host.Reset(1);
	const auto firstToken = host.Open(1, "first", 1);
	REQUIRE(firstToken);
	std::size_t retirements{};
	script->onRetire = [&] {
		++retirements;
		CHECK(host.Close(*firstToken));
	};
	std::optional<std::int32_t> nextToken;
	FakeTimer                   timer;
	std::size_t                 completions{};
	MCMBridge::BridgeErrorCode  error{};
	auto                        operation = std::make_shared<MCMBridge::HostedPageOperation>(
		Descriptor(),
		script,
		"General",
		0,
		false,
		MCMBridge::HostedPageMode::kActivate,
		[] { return false; },
		[&](auto a_result) {
			++completions;
			if (!a_result) {
				error = a_result.error().code;
			}
			CHECK(script->retired);
			const auto admitted = host.Open(1, "second", 2);
			REQUIRE(admitted);
			nextToken = *admitted;
		},
		timer);
	operation->Start();
	SECTION("OpenConfig does not return") {}
	SECTION("SetPage does not return")
	{
		script->CompleteNext();
		REQUIRE(Called(*script, MCMBridge::ClassicMethod::kSetPage));
		timer.RunNext();
		CHECK(completions == 0);
	}
	REQUIRE(timer.tasks.size() == 1);
	timer.RunNext();
	CHECK(completions == 1);
	CHECK(error == MCMBridge::BridgeErrorCode::kTimedOut);
	CHECK(retirements == 1);
	CHECK_FALSE(Called(*script, MCMBridge::ClassicMethod::kCloseConfig));

	script->CompleteNext();
	CHECK(completions == 1);
	CHECK(retirements == 1);
	REQUIRE(nextToken);
	CHECK(host.IsActive(*nextToken));
	CHECK_FALSE(host.IsActive(*firstToken));
	CHECK_FALSE(script->Dispatch({ .method = MCMBridge::ClassicMethod::kOpenConfig }, [] {}));
	CHECK_FALSE(Called(*script, MCMBridge::ClassicMethod::kCloseConfig));
}

TEST_CASE("Completed writes distinguish explicit page resets from unexplained identity loss")
{
	auto script = ScriptWithToggle();
	script->configOpen = true;
	script->deferCallbacks = true;
	const auto              control = script->page.controls.front();
	MCMBridge::WriteCommand command{
		.settingID = control.identity.stableID,
		.expectedIdentity = control.identity,
		.expectedType = control.type,
		.expectedValue = script->currentValue,
		.desiredValue = false
	};
	FakeTimer                                             timer;
	std::optional<MCMBridge::Result<MCMBridge::MCMValue>> result;
	auto                                                  operation = std::make_shared<MCMBridge::ClassicWriteOperation>(
		script, control, command, [] { return false; }, [&](auto a_result) { result = std::move(a_result); },
		timer, MCMBridge::ClassicWriteMode::kHosted);
	operation->Start();
	REQUIRE(CallCount(*script, MCMBridge::ClassicMethod::kSelectOption) == 1);
	bool explicitReset{};
	SECTION("Callback explicitly requests a page reset")
	{
		explicitReset = true;
		++script->resetRevision;
	}
	SECTION("Identity disappears without a reset") {}
	script->matches = false;
	script->CompleteNext();
	script->CompleteNext();
	REQUIRE(result);
	REQUIRE_FALSE(*result);
	CHECK(result->error().code == (explicitReset ? MCMBridge::BridgeErrorCode::kPageRebuilt : MCMBridge::BridgeErrorCode::kStaleSnapshot));
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kSelectOption) == 1);
	CHECK_FALSE(Called(*script, MCMBridge::ClassicMethod::kCloseConfig));
}

TEST_CASE("Classic text activation executes SelectOption and captures the rebuilt value")
{
	auto script = ScriptWithToggle();
	auto control = script->page.controls.front();
	control.type = MCMBridge::MCMControlType::kText;
	control.identity.pageKey = "General";
	control.identity.pageIndex = 0;
	control.value = std::string("$Medium");
	control.writeCapability = MCMBridge::WriteCapability::kWritable;
	script->currentValue = control.value;
	script->selectResult = std::string("$Large");
	MCMBridge::WriteCommand command{
		.snapshotGeneration = 1,
		.settingID = control.identity.stableID,
		.expectedIdentity = control.identity,
		.expectedType = control.type,
		.expectedValue = control.value,
		.desiredValue = std::monostate{},
		.intent = MCMBridge::WriteIntent::kActivate
	};
	FakeTimer                                             timer;
	std::optional<MCMBridge::Result<MCMBridge::MCMValue>> result;
	auto                                                  operation = std::make_shared<MCMBridge::ClassicWriteOperation>(
		script, control, command, [] { return false; }, [&](auto a_value) { result = std::move(a_value); }, timer);
	operation->Start();

	REQUIRE(result);
	REQUIRE(*result);
	CHECK(std::get<std::string>(**result) == "$Large");
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kSelectOption) == 1);
	CHECK(Called(*script, MCMBridge::ClassicMethod::kCloseConfig));
}

TEST_CASE("Classic keymap write executes RemapKey and confirms the key code")
{
	auto script = ScriptWithToggle();
	auto control = script->page.controls.front();
	control.type = MCMBridge::MCMControlType::kKeymap;
	control.identity.pageKey = "Controls";
	control.identity.pageIndex = 1;
	control.identity.optionIndex = 4;
	control.value = static_cast<std::int32_t>(42);
	control.writeCapability = MCMBridge::WriteCapability::kWritable;
	script->currentValue = control.value;
	MCMBridge::WriteCommand command{
		.snapshotGeneration = 1,
		.settingID = control.identity.stableID,
		.expectedIdentity = control.identity,
		.expectedType = control.type,
		.expectedValue = control.value,
		.desiredValue = static_cast<std::int32_t>(57)
	};
	FakeTimer                                             timer;
	std::optional<MCMBridge::Result<MCMBridge::MCMValue>> result;
	auto                                                  operation = std::make_shared<MCMBridge::ClassicWriteOperation>(
		script, control, command, [] { return false; }, [&](auto a_value) { result = std::move(a_value); }, timer);
	operation->Start();

	REQUIRE(result);
	REQUIRE(*result);
	CHECK(std::get<std::int32_t>(**result) == 57);
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kRemapKey) == 1);
	CHECK(Called(*script, MCMBridge::ClassicMethod::kCloseConfig));
}

TEST_CASE("Classic keymap reset executes ResetOption and captures the default key code")
{
	auto script = ScriptWithToggle();
	auto control = script->page.controls.front();
	control.type = MCMBridge::MCMControlType::kKeymap;
	control.identity.pageKey = "Controls";
	control.identity.pageIndex = 1;
	control.identity.optionIndex = 4;
	control.value = static_cast<std::int32_t>(57);
	control.writeCapability = MCMBridge::WriteCapability::kWritable;
	script->currentValue = control.value;
	script->resetResult = static_cast<std::int32_t>(42);
	MCMBridge::WriteCommand command{
		.snapshotGeneration = 1,
		.settingID = control.identity.stableID,
		.expectedIdentity = control.identity,
		.expectedType = control.type,
		.expectedValue = control.value,
		.desiredValue = std::monostate{},
		.intent = MCMBridge::WriteIntent::kReset
	};
	FakeTimer                                             timer;
	std::optional<MCMBridge::Result<MCMBridge::MCMValue>> result;
	auto                                                  operation = std::make_shared<MCMBridge::ClassicWriteOperation>(
		script, control, command, [] { return false; }, [&](auto a_value) { result = std::move(a_value); }, timer);
	operation->Start();

	REQUIRE(result);
	REQUIRE(*result);
	CHECK(std::get<std::int32_t>(**result) == 42);
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kResetOption) == 1);
	CHECK(Called(*script, MCMBridge::ClassicMethod::kCloseConfig));
}

TEST_CASE("MCM Helper stepper advances through its original callback")
{
	auto script = ScriptWithToggle();
	auto control = script->page.controls.front();
	control.type = MCMBridge::MCMControlType::kStepper;
	control.identity.pageKey = "General";
	control.identity.pageIndex = 0;
	control.value = static_cast<std::int32_t>(1);
	control.menu = MCMBridge::MenuMetadata{
		.options = { "$Small", "$Medium", "$Large" },
		.selectedIndex = 1,
		.availability = MCMBridge::MetadataAvailability::kAvailable
	};
	control.writeCapability = MCMBridge::WriteCapability::kWritable;
	script->currentValue = std::string("$Medium");
	script->selectResult = std::string("$Large");
	MCMBridge::WriteCommand command{
		.snapshotGeneration = 1,
		.settingID = control.identity.stableID,
		.expectedIdentity = control.identity,
		.expectedType = control.type,
		.expectedValue = control.value,
		.desiredValue = std::monostate{},
		.intent = MCMBridge::WriteIntent::kActivate
	};
	FakeTimer                                             timer;
	std::optional<MCMBridge::Result<MCMBridge::MCMValue>> result;
	auto                                                  operation = std::make_shared<MCMBridge::ClassicWriteOperation>(
		script, control, command, [] { return false; }, [&](auto a_value) { result = std::move(a_value); }, timer);
	operation->Start();

	REQUIRE(result);
	REQUIRE(*result);
	CHECK(std::get<std::int32_t>(**result) == 2);
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kSelectOption) == 1);
}

TEST_CASE("MCM Helper color write uses exact dialog values")
{
	auto script = ScriptWithToggle();
	auto control = script->page.controls.front();
	control.type = MCMBridge::MCMControlType::kColor;
	control.identity.pageKey = "General";
	control.identity.pageIndex = 0;
	control.value = static_cast<std::uint32_t>(0x00112233U);
	control.writeCapability = MCMBridge::WriteCapability::kWritable;
	script->currentValue = control.value;
	script->colorMetadata = MCMBridge::ColorMetadata{
		.start = 0x00112233U,
		.defaultValue = 0x00445566U,
		.availability = MCMBridge::MetadataAvailability::kAvailable
	};
	MCMBridge::WriteCommand command{
		.snapshotGeneration = 1,
		.settingID = control.identity.stableID,
		.expectedIdentity = control.identity,
		.expectedType = control.type,
		.expectedValue = control.value,
		.desiredValue = static_cast<std::uint32_t>(0x00AABBCCU)
	};
	FakeTimer                                             timer;
	std::optional<MCMBridge::Result<MCMBridge::MCMValue>> result;
	auto                                                  operation = std::make_shared<MCMBridge::ClassicWriteOperation>(
		script, control, command, [] { return false; }, [&](auto a_value) { result = std::move(a_value); }, timer);
	operation->Start();

	REQUIRE(result);
	REQUIRE(*result);
	CHECK(std::get<std::uint32_t>(**result) == 0x00AABBCCU);
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kRequestColorDialogData) == 2);
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kSetColorValue) == 1);
}

TEST_CASE("MCM Helper input write uses the original input callback")
{
	auto script = ScriptWithToggle();
	auto control = script->page.controls.front();
	control.type = MCMBridge::MCMControlType::kInput;
	control.identity.pageKey = "General";
	control.identity.pageIndex = 0;
	control.value = std::string("Before");
	control.writeCapability = MCMBridge::WriteCapability::kWritable;
	script->currentValue = control.value;
	MCMBridge::WriteCommand command{
		.snapshotGeneration = 1,
		.settingID = control.identity.stableID,
		.expectedIdentity = control.identity,
		.expectedType = control.type,
		.expectedValue = control.value,
		.desiredValue = std::string("After")
	};
	FakeTimer                                             timer;
	std::optional<MCMBridge::Result<MCMBridge::MCMValue>> result;
	auto                                                  operation = std::make_shared<MCMBridge::ClassicWriteOperation>(
		script, control, command, [] { return false; }, [&](auto a_value) { result = std::move(a_value); }, timer);
	operation->Start();

	REQUIRE(result);
	REQUIRE(*result);
	CHECK(std::get<std::string>(**result) == "After");
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kRequestInputDialogData) == 1);
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kSetInputText) == 1);
}

TEST_CASE("MCM Helper value reset executes the original default callback")
{
	auto script = ScriptWithToggle();
	auto control = script->page.controls.front();
	control.identity.pageKey = "General";
	control.identity.pageIndex = 0;
	control.value = false;
	control.defaultValue = true;
	control.writeCapability = MCMBridge::WriteCapability::kWritable;
	script->currentValue = control.value;
	script->resetResult = true;
	MCMBridge::WriteCommand command{
		.snapshotGeneration = 1,
		.settingID = control.identity.stableID,
		.expectedIdentity = control.identity,
		.expectedType = control.type,
		.expectedValue = control.value,
		.desiredValue = std::monostate{},
		.intent = MCMBridge::WriteIntent::kReset
	};
	FakeTimer                                             timer;
	std::optional<MCMBridge::Result<MCMBridge::MCMValue>> result;
	auto                                                  operation = std::make_shared<MCMBridge::ClassicWriteOperation>(
		script, control, command, [] { return false; }, [&](auto a_value) { result = std::move(a_value); }, timer);
	operation->Start();

	REQUIRE(result);
	REQUIRE(*result);
	CHECK(std::get<bool>(**result));
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kResetOption) == 1);
}

TEST_CASE("Classic scan reads exact color dialog metadata")
{
	auto script = ScriptWithToggle();
	auto color = script->page.controls.front();
	color.type = MCMBridge::MCMControlType::kColor;
	color.value = static_cast<std::uint32_t>(0U);
	color.identity.optionIndex = 0;
	color.writeCapability = MCMBridge::WriteCapability::kWritable;
	script->page.controls.front() = color;
	script->colorMetadata = MCMBridge::ColorMetadata{
		.start = 0x00112233U,
		.defaultValue = 0x00445566U,
		.availability = MCMBridge::MetadataAvailability::kAvailable
	};
	FakeTimer                                           timer;
	MCMBridge::ScopedMenuOptionResolver                 resolver;
	std::optional<MCMBridge::Result<MCMBridge::MCMMod>> result;
	auto                                                operation = std::make_shared<MCMBridge::ClassicScanOperation>(
		Descriptor(), script, resolver, [] { return false; }, [&](auto a_value) { result = std::move(a_value); }, timer);
	operation->Start();

	REQUIRE(result);
	REQUIRE(*result);
	const auto& scanned = (*result)->pages.front().controls.front();
	CHECK(std::get<std::uint32_t>(scanned.value) == 0x00112233U);
	CHECK(std::get<std::uint32_t>(*scanned.defaultValue) == 0x00445566U);
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kRequestColorDialogData) == 1);
}

TEST_CASE("Classic scan reads input start text metadata")
{
	auto script = ScriptWithToggle();
	auto input = script->page.controls.front();
	input.type = MCMBridge::MCMControlType::kInput;
	input.value = std::string("Current");
	input.identity.optionIndex = 0;
	input.writeCapability = MCMBridge::WriteCapability::kWritable;
	script->page.controls.front() = input;
	script->inputMetadata = MCMBridge::InputMetadata{
		.startText = "Suggested",
		.availability = MCMBridge::MetadataAvailability::kAvailable
	};
	FakeTimer                                           timer;
	MCMBridge::UnavailableMenuOptionResolver            resolver;
	std::optional<MCMBridge::Result<MCMBridge::MCMMod>> result;
	auto                                                operation = std::make_shared<MCMBridge::ClassicScanOperation>(
		Descriptor(), script, resolver, [] { return false; }, [&](auto a_value) { result = std::move(a_value); }, timer);
	operation->Start();

	REQUIRE(result);
	REQUIRE(*result);
	const auto& scanned = (*result)->pages.front().controls.front();
	REQUIRE(scanned.input);
	CHECK(scanned.input->startText == "Suggested");
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kRequestInputDialogData) == 1);
}

TEST_CASE("Hosted page activation preserves the MCM title")
{
	auto script = ScriptWithToggle();
	script->pageTitle = "$CustomTitle";
	FakeTimer                                            timer;
	std::optional<MCMBridge::Result<MCMBridge::MCMPage>> result;
	auto                                                 operation = std::make_shared<MCMBridge::HostedPageOperation>(
		Descriptor(), script, "General", 0, false, MCMBridge::HostedPageMode::kActivate,
		[] { return false; }, [&](auto a_value) { result = std::move(a_value); }, timer);
	operation->Start();

	REQUIRE(result);
	REQUIRE(*result);
	CHECK((*result)->title == "$CustomTitle");
}

TEST_CASE("Classic help invokes the original highlight callback")
{
	auto script = ScriptWithToggle();
	script->configOpen = true;
	script->infoText = "$HelpText";
	FakeTimer                                     timer;
	std::optional<MCMBridge::Result<std::string>> result;
	auto                                          operation = std::make_shared<MCMBridge::ClassicHelpOperation>(
		script, script->page.controls.front().identity, [] { return false; },
		[&](auto a_value) { result = std::move(a_value); }, timer);
	operation->Start();

	REQUIRE(result);
	REQUIRE(*result);
	CHECK(**result == "$HelpText");
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kHighlightOption) == 1);
}

TEST_CASE("Direct host calls preserve 2233 client decisions without extra scans or callbacks")
{
	auto        script = ScriptWithToggle();
	FakeTimer   timer;
	auto        session = std::make_shared<MCMBridge::HostCallSession>(script, timer);
	std::size_t completed{};
	auto        complete = [&](MCMBridge::HostCallStatus a_status) {
		CHECK(a_status == MCMBridge::HostCallStatus::kCompleted);
		++completed;
	};
	REQUIRE(session->Submit({ MCMBridge::ClassicMethod::kOpenConfig }, complete, std::chrono::seconds(10)));
	for (int index = 0; index < 2233; ++index)
		REQUIRE(session->Submit({ MCMBridge::ClassicMethod::kSelectOption }, complete, std::chrono::seconds(10)));
	REQUIRE(session->Submit({ MCMBridge::ClassicMethod::kCloseConfig }, complete, std::chrono::seconds(10)));
	CHECK(completed == 2235);
	CHECK(script->calls.size() == completed);
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kOpenConfig) == 1);
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kCloseConfig) == 1);
	CHECK(script->readPages.empty());
	CHECK(timer.tasks.empty());
}

namespace
{
	class SettlementTimer final : public MCMBridge::IOperationTimer
	{
	public:
		void          After(std::chrono::milliseconds a_delay, std::function<void()> a_task) override { Schedule(a_delay, std::move(a_task)); }
		std::uint64_t Schedule(std::chrono::milliseconds a_delay, std::function<void()> a_task) override
		{
			const auto handle = ++serial;
			tasks.emplace(handle, Task{ now + a_delay, std::move(a_task) });
			return handle;
		}
		void Cancel(std::uint64_t a_handle) override { tasks.erase(a_handle); }
		void RunNext()
		{
			REQUIRE_FALSE(tasks.empty());
			auto next = std::min_element(tasks.begin(), tasks.end(), [](const auto& a_left, const auto& a_right) { return a_left.second.due < a_right.second.due; });
			now = next->second.due;
			auto task = std::move(next->second.run);
			tasks.erase(next);
			task();
		}
		struct Task
		{
			std::chrono::milliseconds due;
			std::function<void()>     run;
		};
		std::chrono::milliseconds     now{};
		std::uint64_t                 serial{};
		std::map<std::uint64_t, Task> tasks;
	};

	std::shared_ptr<FakeScript> ScriptWithTextSelection()
	{
		auto script = ScriptWithToggle();
		script->configOpen = true;
		script->trackCurrentPage = true;
		script->currentPage = MCMBridge::ClassicPageSelection{ "General", 0 };
		script->selectionControl = script->page.controls.front();
		script->selectionControl->type = MCMBridge::MCMControlType::kText;
		script->selectionControl->rawLabel = "$Activation";
		script->selectionControl->value = std::string("Disabled");
		return script;
	}
}

TEST_CASE("Disabled and hidden native selections never invoke a mod callback")
{
	auto script = ScriptWithTextSelection();
	SECTION("Disabled") { script->selectionControl->disabled = true; }
	SECTION("Hidden") { script->selectionControl->hidden = true; }
	SettlementTimer                        timer;
	auto                                   session = std::make_shared<MCMBridge::HostCallSession>(script, timer);
	std::vector<MCMBridge::HostCallStatus> results;
	REQUIRE(session->Submit({ MCMBridge::ClassicMethod::kSelectOption }, [&](auto a_status) { results.push_back(a_status); }, std::chrono::seconds(30)));
	REQUIRE(results.size() == 1);
	CHECK(results.front() == MCMBridge::HostCallStatus::kControlUnavailable);
	CHECK(script->calls.empty());
	CHECK(timer.tasks.empty());
}

TEST_CASE("Text activation refresh is client driven and never repeats the click")
{
	auto script = ScriptWithTextSelection();
	bool ready{};
	script->onComplete = [&](auto a_method) {
		if (a_method == MCMBridge::ClassicMethod::kSelectOption) {
			script->selectionControl->disabled = true;
			script->selectionControl->value = std::string("Starting");
		}
		if (a_method == MCMBridge::ClassicMethod::kSetPage && ready)
			script->selectionControl->value = std::string("Enabled");
		if (a_method == MCMBridge::ClassicMethod::kOpenConfig && ready)
			script->selectionControl->disabled = false;
	};
	SettlementTimer timer;
	auto            session = std::make_shared<MCMBridge::HostCallSession>(script, timer);
	std::size_t     completed{};
	const auto      complete = [&](auto a_status) {
		CHECK(a_status == MCMBridge::HostCallStatus::kCompleted);
		++completed;
	};
	REQUIRE(session->Submit({ MCMBridge::ClassicMethod::kSelectOption }, complete, std::chrono::seconds(30)));
	CHECK(completed == 1);
	CHECK(script->calls.size() == 1);
	CHECK(timer.tasks.empty());
	for (int index = 0; index < 4; ++index)
		REQUIRE(session->Submit({ MCMBridge::ClassicMethod::kSetPage, "General", "", 0 }, complete, std::chrono::seconds(30)));
	CHECK(completed == 5);
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kOpenConfig) == 1);
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kCloseConfig) == 1);
	ready = true;
	REQUIRE(session->Submit({ MCMBridge::ClassicMethod::kSetPage, "General", "", 0 }, complete, std::chrono::seconds(30)));
	CHECK(completed == 6);
	CHECK_FALSE(script->selectionControl->disabled);
	CHECK(std::get<std::string>(script->selectionControl->value) == "Enabled");
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kSelectOption) == 1);
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kOpenConfig) == 2);
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kCloseConfig) == 2);
	CHECK(timer.tasks.empty());
}

TEST_CASE("Ordinary text cyclers complete without additional navigation")
{
	auto script = ScriptWithTextSelection();
	script->onComplete = [&](auto) { script->selectionControl->value = std::string("Next"); };
	SettlementTimer timer;
	auto            session = std::make_shared<MCMBridge::HostCallSession>(script, timer);
	std::size_t     completed{};
	REQUIRE(session->Submit({ MCMBridge::ClassicMethod::kSelectOption }, [&](auto a_status) {
  CHECK(a_status == MCMBridge::HostCallStatus::kCompleted);
  ++completed; }, std::chrono::seconds(30)));
	CHECK(completed == 1);
	CHECK(script->calls.size() == 1);
	CHECK(timer.tasks.empty());
}

TEST_CASE("ForcePageReset does not create an independent activation refresh loop")
{
	auto script = ScriptWithTextSelection();
	script->onComplete = [&](auto a_method) {
		if (a_method == MCMBridge::ClassicMethod::kSelectOption)
			++script->resetRevision;
	};
	SettlementTimer timer;
	auto            session = std::make_shared<MCMBridge::HostCallSession>(script, timer);
	std::size_t     completed{};
	REQUIRE(session->Submit({ MCMBridge::ClassicMethod::kSelectOption }, [&](auto a_status) {
  CHECK(a_status == MCMBridge::HostCallStatus::kCompleted);
  ++completed; }, std::chrono::seconds(30)));
	CHECK(completed == 1);
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kSetPage) == 0);
	CHECK(timer.tasks.empty());
}

TEST_CASE("Text transition reopening drains callbacks without repeating mutations")
{
	auto script = ScriptWithTextSelection();
	script->onComplete = [&](auto a_method) {
		if (a_method == MCMBridge::ClassicMethod::kSelectOption) {
			script->selectionControl->disabled = true;
			script->selectionControl->value = std::string("Starting");
		}
	};
	SettlementTimer timer;
	auto            session = std::make_shared<MCMBridge::HostCallSession>(script, timer);
	REQUIRE(session->Submit({ MCMBridge::ClassicMethod::kSelectOption }, [](auto) {}, std::chrono::seconds(30)));
	script->deferCallbacks = true;
	std::vector<MCMBridge::HostCallStatus> results;
	REQUIRE(session->Submit({ MCMBridge::ClassicMethod::kSetPage, "General", "", 0 }, [&](auto a_status) { results.push_back(a_status); }, std::chrono::milliseconds(500)));
	script->CompleteNext();  // The requested page exposes the disabled transition.
	script->CompleteNext();  // Close completes; Open is now in flight.
	CHECK(results.empty());
	SECTION("Cancellation drains the original callback")
	{
		session->Cancel();
		CHECK(results.empty());
		script->CompleteNext();
		REQUIRE(results.size() == 1);
		CHECK(results.front() == MCMBridge::HostCallStatus::kCancelled);
	}
	SECTION("Save change rejects a late completion")
	{
		session->Invalidate();
		script->CompleteNext();
		REQUIRE(results.size() == 1);
		CHECK(results.front() == MCMBridge::HostCallStatus::kSessionInvalidated);
	}
	SECTION("Timeout rejects a late completion")
	{
		timer.RunNext();
		script->CompleteNext();
		REQUIRE(results.size() == 1);
		CHECK(results.front() == MCMBridge::HostCallStatus::kTimedOut);
	}
	SECTION("Navigation changes cannot redirect the requested page")
	{
		script->pages = { "Other" };
		script->CompleteNext();
		REQUIRE(results.size() == 1);
		CHECK(results.front() == MCMBridge::HostCallStatus::kControlUnavailable);
	}
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kSelectOption) == 1);
	CHECK(CallCount(*script, MCMBridge::ClassicMethod::kSetPage) == 1);
	CHECK(script->callbacks.empty());
	CHECK(timer.tasks.empty());
}

TEST_CASE("Read and lifecycle calls do not acquire the host write pause")
{
	using enum MCMBridge::ClassicMethod;
	for (const auto method : { kSelectOption, kResetOption, kSetSliderValue, kSetMenuIndex, kSetColorValue, kSetInputText, kRemapKey, kSetModSettingInt, kOnSettingChange })
		CHECK(MCMBridge::RequiresHostWritePause(method));
	for (const auto method : { kOpenConfig, kCloseConfig, kSetPage, kHighlightOption, kRequestSliderDialogData, kRequestMenuDialogData, kRequestColorDialogData, kRequestInputDialogData })
		CHECK_FALSE(MCMBridge::RequiresHostWritePause(method));
}

TEST_CASE("Direct host navigation completes only with matching ready page data")
{
	auto script = ScriptWithToggle();
	script->deferCallbacks = true;
	script->trackCurrentPage = true;
	FakeTimer                              timer;
	auto                                   session = std::make_shared<MCMBridge::HostCallSession>(script, timer);
	std::vector<MCMBridge::HostCallStatus> results;
	REQUIRE(session->Submit({ MCMBridge::ClassicMethod::kSetPage, "General", "", 0 }, [&](auto a_status) { results.push_back(a_status); }, std::chrono::seconds(10)));
	CHECK(results.empty());
	CHECK(session->Busy());
	SECTION("Requested page")
	{
		script->callbacks.front()();
		REQUIRE(results.size() == 1);
		CHECK(results.front() == MCMBridge::HostCallStatus::kCompleted);
	}
	SECTION("Different page is not a completed selection")
	{
		script->onComplete = [&](auto) { script->currentPage = MCMBridge::ClassicPageSelection{ "Other", 1 }; };
		script->callbacks.front()();
		REQUIRE(results.size() == 1);
		CHECK(results.front() == MCMBridge::HostCallStatus::kInvalidData);
	}
	script->callbacks.front()();
	timer.RunNext();
	CHECK(results.size() == 1);
}

TEST_CASE("Direct host cancellation retains the running call without competing cleanup")
{
	auto script = ScriptWithToggle();
	script->deferCallbacks = true;
	FakeTimer                              timer;
	auto                                   session = std::make_shared<MCMBridge::HostCallSession>(script, timer);
	std::vector<MCMBridge::HostCallStatus> results;
	auto                                   complete = [&](auto a_status) { results.push_back(a_status); };
	REQUIRE(session->Submit({ MCMBridge::ClassicMethod::kSelectOption }, complete, std::chrono::seconds(10)));
	CHECK_FALSE(session->Submit({ MCMBridge::ClassicMethod::kSelectOption }, complete, std::chrono::seconds(10)));
	session->Cancel();
	CHECK(session->Busy());
	CHECK(results.empty());
	CHECK_FALSE(session->Submit({ MCMBridge::ClassicMethod::kCloseConfig }, complete, std::chrono::seconds(10)));
	SECTION("Original callback completes")
	{
		script->callbacks.front()();
		REQUIRE(results.size() == 1);
		CHECK(results.front() == MCMBridge::HostCallStatus::kCancelled);
	}
	SECTION("Timeout does not repeat the call")
	{
		timer.RunNext();
		REQUIRE(results.size() == 1);
		CHECK(results.front() == MCMBridge::HostCallStatus::kTimedOut);
		CHECK(script->retired);
	}
	SECTION("Save invalidation ends the old call immediately")
	{
		session->Invalidate();
		REQUIRE(results.size() == 1);
		CHECK(results.front() == MCMBridge::HostCallStatus::kSessionInvalidated);
		CHECK(script->retired);
	}
	script->callbacks.front()();
	CHECK(results.size() == 1);
	CHECK_FALSE(session->Busy());
	CHECK(script->calls.size() == 1);
}

TEST_CASE("Direct host completion can submit its successor without inheriting an old timeout")
{
	auto                                   script = ScriptWithToggle();
	FakeTimer                              timer;
	auto                                   session = std::make_shared<MCMBridge::HostCallSession>(script, timer);
	std::vector<MCMBridge::HostCallStatus> results;
	REQUIRE(session->Submit({ MCMBridge::ClassicMethod::kOpenConfig }, [&](auto a_status) {
		results.push_back(a_status);
		script->deferCallbacks = true;
		REQUIRE(session->Submit({ MCMBridge::ClassicMethod::kSelectOption },
			[&](auto a_nextStatus) { results.push_back(a_nextStatus); }, std::chrono::seconds(10))); }, std::chrono::seconds(10)));
	CHECK(results.size() == 1);
	CHECK(timer.tasks.size() == 1);
	script->callbacks.front()();
	timer.RunNext();
	REQUIRE(results.size() == 2);
	CHECK(results.back() == MCMBridge::HostCallStatus::kCompleted);
}

TEST_CASE("Direct host reports dispatch failure and owner destruction exactly once")
{
	auto                                   script = ScriptWithToggle();
	FakeTimer                              timer;
	auto                                   session = std::make_shared<MCMBridge::HostCallSession>(script, timer);
	std::vector<MCMBridge::HostCallStatus> results;
	SECTION("Dispatch failure")
	{
		script->failedMethod = MCMBridge::ClassicMethod::kOpenConfig;
		REQUIRE(session->Submit({ MCMBridge::ClassicMethod::kOpenConfig }, [&](auto a_status) { results.push_back(a_status); }, std::chrono::seconds(10)));
		REQUIRE(results.size() == 1);
		CHECK(results.front() == MCMBridge::HostCallStatus::kDispatchFailed);
		CHECK(timer.tasks.empty());
	}
	SECTION("Owner destruction")
	{
		script->deferCallbacks = true;
		REQUIRE(session->Submit({ MCMBridge::ClassicMethod::kOpenConfig }, [&](auto a_status) { results.push_back(a_status); }, std::chrono::seconds(10)));
		session.reset();
		script->callbacks.front()();
		timer.RunNext();
		REQUIRE(results.size() == 1);
		CHECK(results.front() == MCMBridge::HostCallStatus::kSessionInvalidated);
	}
}

TEST_CASE("Direct host cleanup waits for cancellation and cannot close a timed out adapter")
{
	auto script = ScriptWithToggle();
	script->configOpen = true;
	script->deferCallbacks = true;
	FakeTimer                              timer;
	auto                                   session = std::make_shared<MCMBridge::HostCallSession>(script, timer);
	std::vector<MCMBridge::HostCallStatus> results;
	auto                                   complete = [&](auto a_status) { results.push_back(a_status); };
	REQUIRE(session->Submit({ MCMBridge::ClassicMethod::kSelectOption }, complete, std::chrono::seconds(10)));
	session->Cancel();
	CHECK_FALSE(session->Close(complete, std::chrono::seconds(10)));
	SECTION("Safe close after the running callback")
	{
		auto callback = std::move(script->callbacks.front());
		script->callbacks.pop_front();
		callback();
		REQUIRE(session->Close(complete, std::chrono::seconds(10)));
		script->callbacks.front()();
		CHECK_FALSE(script->configOpen);
		CHECK(CallCount(*script, MCMBridge::ClassicMethod::kCloseConfig) == 1);
		REQUIRE(results.size() == 2);
		CHECK(results.back() == MCMBridge::HostCallStatus::kCompleted);
		CHECK(session->Stopped());
	}
	SECTION("No competing cleanup after timeout")
	{
		timer.RunNext();
		CHECK_FALSE(session->Close(complete, std::chrono::seconds(10)));
		CHECK(CallCount(*script, MCMBridge::ClassicMethod::kCloseConfig) == 0);
		REQUIRE(results.size() == 1);
		CHECK(results.front() == MCMBridge::HostCallStatus::kTimedOut);
	}
}
