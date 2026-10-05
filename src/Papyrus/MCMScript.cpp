#include "MCMBridge/Papyrus/MCMScript.h"

#include "MCMBridge/Papyrus/SkyUIFrontendState.h"

#include <atomic>
#include <format>
#include <nlohmann/json.hpp>

namespace MCMBridge
{
	MCMScript::MCMScript(RE::BSTSmartPointer<RE::BSScript::Object> a_script) :
		script(std::move(a_script))
	{}

	bool MCMScript::Dispatch(ClassicCall a_call, Continuation a_continuation)
	{
		switch (a_call.method) {
		case ClassicMethod::kOpenConfig:
		case ClassicMethod::kCloseConfig:
			return Call(ClassicMethodName(a_call.method), RE::MakeFunctionArguments(), std::move(a_continuation));
		case ClassicMethod::kSetPage:
			if (spdlog::should_log(spdlog::level::debug))
				SKSE::log::debug("MCM page request: script={} requested={} index={} live_pages={}",
					script->GetTypeInfo()->GetName(), nlohmann::json(a_call.text).dump(), a_call.integer,
					nlohmann::json(ReadPages()).dump());
			return Call("SetPage", RE::MakeFunctionArguments(std::move(a_call.text), static_cast<std::int32_t>(a_call.integer)), std::move(a_continuation));
		case ClassicMethod::kRequestSliderDialogData:
		case ClassicMethod::kRequestMenuDialogData:
		case ClassicMethod::kRequestColorDialogData:
		case ClassicMethod::kRequestInputDialogData:
		case ClassicMethod::kSelectOption:
		case ClassicMethod::kResetOption:
		case ClassicMethod::kHighlightOption:
		case ClassicMethod::kSetMenuIndex:
		case ClassicMethod::kSetColorValue:
			return Call(ClassicMethodName(a_call.method), RE::MakeFunctionArguments(static_cast<std::int32_t>(a_call.integer)), std::move(a_continuation));
		case ClassicMethod::kSetSliderValue:
			return Call("SetSliderValue", RE::MakeFunctionArguments(static_cast<float>(a_call.number)), std::move(a_continuation));
		case ClassicMethod::kSetInputText:
			return Call("SetInputText", RE::MakeFunctionArguments(std::move(a_call.text)), std::move(a_continuation));
		case ClassicMethod::kRemapKey:
			return Call(
				"RemapKey",
				RE::MakeFunctionArguments(
					static_cast<std::int32_t>(a_call.integer),
					static_cast<std::int32_t>(a_call.secondaryInteger),
					std::move(a_call.text),
					std::move(a_call.secondaryText)),
				std::move(a_continuation));
		}
		return false;
	}

	bool MCMScript::Call(
		std::string_view                  a_functionName,
		RE::BSScript::IFunctionArguments* a_arguments,
		SKSE::TaskInterface::TaskFn       a_continuation) const
	{
		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		if (!vm || !script) {
			return false;
		}

		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
		const auto                                               operation = ++hostObservation->operation;
		hostObservation->last = {};
		if (a_functionName == "OpenConfig" || a_functionName == "SetPage" || a_functionName == "CloseConfig")
			hostObservation->pageState.Invalidate();
		hostObservation->pendingPageChanges = false;
		hostObservation->completed = false;
		if (a_continuation) {
			callback.reset(new NativeHostCallback(script,
				ReadScalarString("JOURNAL_MENU").value_or("Journal Menu"), ReadScalarString("MENU_ROOT").value_or("_root.ConfigPanelFader.configPanel"),
				[weak = std::weak_ptr(hostObservation), operation, method = std::string(a_functionName), next = std::move(a_continuation)](NativeHostObservation a_observation) mutable {
					if (auto state = weak.lock(); state && state->operation == operation) {
						static std::atomic<bool> reportedCapture{};
						if (a_observation.page && !reportedCapture.exchange(true))
							SKSE::log::info("Native host first complete page capture: method={} controls={}", method, a_observation.page->buffers.optionFlags.size());
						SKSE::log::debug("Native host call: method={} dispatch_ms={} game_task_wait_ms={} page_captured={}", method,
							std::chrono::duration<double, std::milli>(a_observation.dispatchTime).count(),
							std::chrono::duration<double, std::milli>(a_observation.taskWait).count(), static_cast<bool>(a_observation.page));
						state->last = std::move(a_observation);
						if (state->last.page) {
							state->pageState.Invalidate();
							state->pendingPageChanges = true;
						} else {
							if (!state->pageState.Apply(state->last.changes) && !state->last.changes.controls.empty())
								state->last.changes.malformed = true;
						}
						state->completed = true;
					}
					next();
				}));
		}
		auto boundScript = script;
		return vm->DispatchMethodCall(boundScript, RE::BSFixedString(a_functionName), a_arguments, callback);
	}

	std::vector<std::string> MCMScript::ReadPages() const
	{
		return ReadNavigationPages().value_or(std::vector<std::string>{});
	}

	std::optional<std::vector<std::string>> MCMScript::ReadNavigationPages() const
	{
		std::vector<std::string> pages;
		const auto*              property = FindVariable("Pages");
		if (!property || property->GetType().GetRawType() != RE::BSScript::TypeInfo::RawType::kStringArray) {
			return std::nullopt;
		}
		const auto values = property->GetArray();
		// Classic menus may leave Pages unallocated and use only OnPageReset("").
		if (!values)
			return pages;
		pages.reserve(values->size());
		for (const auto& value : *values) {
			if (!value.IsString())
				return std::nullopt;
			pages.emplace_back(value.GetString());
		}
		return pages;
	}

	std::optional<ClassicPageSelection> MCMScript::ReadCurrentPage() const
	{
		const auto pageNumber = ReadInteger("_currentPageNum");
		const auto pageName = ReadScalarString("_currentPage");
		if (!IsConfigOpen() || !pageNumber || *pageNumber < 0 || !pageName) {
			return std::nullopt;
		}
		ClassicPageSelection page{ *pageName, *pageNumber - 1 };
		if (page.index < 0) {
			return page.name.empty() ? std::optional<ClassicPageSelection>(std::move(page)) : std::nullopt;
		}
		const auto registeredName = ReadString("Pages", static_cast<std::size_t>(page.index));
		return registeredName && *registeredName == page.name ?
		           std::optional<ClassicPageSelection>(std::move(page)) :
		           std::nullopt;
	}

	void MCMScript::BeginPageCapture()
	{
		capturedPageRevision = SkyUIFrontendState::GetSingleton().PageRevision();
		SkyUIFrontendState::GetSingleton().BeginPageCapture();
	}

	bool MCMScript::CanReusePage() const
	{
		return !hostObservation->last.changes.malformed && !hostObservation->last.changes.resetRequested &&
		       capturedPageRevision == SkyUIFrontendState::GetSingleton().PageRevision();
	}

	bool MCMScript::CanReuseOpeningPage() const
	{
		return hostObservation->last.page && !hostObservation->last.changes.resetRequested &&
		       !hostObservation->last.changes.malformed && CanReusePage();
	}

	std::uint64_t MCMScript::PageRevision() const
	{
		return SkyUIFrontendState::GetSingleton().PageRevision();
	}

	void MCMScript::BeginInfoCapture()
	{
		SkyUIFrontendState::GetSingleton().BeginInfoCapture();
	}

	std::string MCMScript::ReadPageTitle() const
	{
		return SkyUIFrontendState::GetSingleton().PageTitle();
	}

	std::string MCMScript::ReadInfoText() const
	{
		return SkyUIFrontendState::GetSingleton().InfoText();
	}

	std::optional<MCMValue> MCMScript::ReadValue(MCMControlType a_type, std::uint16_t a_optionIndex) const
	{
		switch (a_type) {
		case MCMControlType::kToggle:
			if (const auto value = ReadNumber("_numValueBuf", a_optionIndex)) {
				return MCMValue(*value != 0.0F);
			}
			break;
		case MCMControlType::kSlider:
			if (const auto value = ReadNumber("_numValueBuf", a_optionIndex)) {
				return MCMValue(*value);
			}
			break;
		case MCMControlType::kKeymap:
			if (const auto value = ReadNumber("_numValueBuf", a_optionIndex)) {
				return MCMValue(static_cast<std::int32_t>(*value));
			}
			break;
		case MCMControlType::kColor:
			if (const auto value = ReadNumber("_numValueBuf", a_optionIndex)) {
				return MCMValue(static_cast<std::uint32_t>(*value));
			}
			break;
		case MCMControlType::kText:
		case MCMControlType::kMenu:
		case MCMControlType::kInput:
			if (const auto value = ReadString("_strValueBuf", a_optionIndex)) {
				return MCMValue(*value);
			}
			break;
		default:
			break;
		}
		return std::nullopt;
	}

	bool MCMScript::Matches(const SettingIdentity& a_identity, MCMControlType a_type) const
	{
		auto       flags = ReadNumber("_optionFlagsBuf", a_identity.optionIndex);
		const auto expectedSkyUIType = a_type == MCMControlType::kToggle                                     ? 3 :
		                               a_type == MCMControlType::kSlider                                     ? 4 :
		                               a_type == MCMControlType::kMenu                                       ? 5 :
		                               a_type == MCMControlType::kColor                                      ? 6 :
		                               a_type == MCMControlType::kKeymap                                     ? 7 :
		                               a_type == MCMControlType::kInput                                      ? 8 :
		                               a_type == MCMControlType::kText || a_type == MCMControlType::kStepper ? 2 :
		                                                                                                       -1;
		if (!flags || (static_cast<std::int32_t>(*flags) & 0xFF) != expectedSkyUIType) {
			return false;
		}
		if (a_identity.stateName.empty()) {
			return true;
		}
		const auto state = ReadString("_stateOptionMap", a_identity.optionIndex);
		return state && *state == a_identity.stateName;
	}

	bool MCMScript::IsConfigOpen() const
	{
		auto       flags = ReadArray("_optionFlagsBuf");
		const auto state = ReadInteger("_state");
		return flags && flags->size() >= 128 && state && *state == 0;
	}

	bool MCMScript::IsPageReady(std::int32_t a_pageIndex) const
	{
		const auto currentPage = ReadInteger("_currentPageNum");
		const auto state = ReadInteger("_state");
		return currentPage && *currentPage == a_pageIndex + 1 && state && *state == 0;
	}

	bool MCMScript::IsMenuReady(std::uint16_t a_optionIndex) const
	{
		const auto currentPage = ReadInteger("_currentPageNum");
		const auto activeOption = ReadInteger("_activeOption");
		const auto state = ReadInteger("_state");
		return currentPage && activeOption && *activeOption == static_cast<std::int32_t>(a_optionIndex) + *currentPage * 256 && state && *state == 0;
	}

}
