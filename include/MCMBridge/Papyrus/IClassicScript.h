#pragma once

#include "MCMBridge/Core/ClassicPageList.h"
#include "MCMBridge/Core/ClassicParser.h"

#include <chrono>
#include <functional>
#include <string_view>

namespace MCMBridge
{
	enum class ClassicMethod
	{
		kOpenConfig,
		kSetPage,
		kRequestSliderDialogData,
		kRequestMenuDialogData,
		kRequestColorDialogData,
		kRequestInputDialogData,
		kSelectOption,
		kResetOption,
		kSetSliderValue,
		kSetMenuIndex,
		kSetColorValue,
		kSetInputText,
		kRemapKey,
		kHighlightOption,
		kCloseConfig,
		kSetModSettingInt,
		kOnSettingChange
	};

	struct ClassicCall
	{
		ClassicMethod method{ ClassicMethod::kOpenConfig };
		std::string   text;
		std::string   secondaryText;
		std::int32_t  integer{};
		std::int32_t  secondaryInteger{};
		float         number{};
	};

	constexpr std::string_view ClassicMethodName(ClassicMethod a_method)
	{
		switch (a_method) {
		case ClassicMethod::kOpenConfig:
			return "OpenConfig";
		case ClassicMethod::kSetPage:
			return "SetPage";
		case ClassicMethod::kRequestSliderDialogData:
			return "RequestSliderDialogData";
		case ClassicMethod::kRequestMenuDialogData:
			return "RequestMenuDialogData";
		case ClassicMethod::kRequestColorDialogData:
			return "RequestColorDialogData";
		case ClassicMethod::kRequestInputDialogData:
			return "RequestInputDialogData";
		case ClassicMethod::kSelectOption:
			return "SelectOption";
		case ClassicMethod::kResetOption:
			return "ResetOption";
		case ClassicMethod::kSetSliderValue:
			return "SetSliderValue";
		case ClassicMethod::kSetMenuIndex:
			return "SetMenuIndex";
		case ClassicMethod::kSetColorValue:
			return "SetColorValue";
		case ClassicMethod::kSetInputText:
			return "SetInputText";
		case ClassicMethod::kRemapKey:
			return "RemapKey";
		case ClassicMethod::kHighlightOption:
			return "HighlightOption";
		case ClassicMethod::kCloseConfig:
			return "CloseConfig";
		case ClassicMethod::kSetModSettingInt:
			return "SetModSettingInt";
		case ClassicMethod::kOnSettingChange:
			return "OnSettingChange";
		}
		return "Unknown";
	}

	constexpr bool RequiresHostWritePause(ClassicMethod a_method)
	{
		switch (a_method) {
		case ClassicMethod::kSelectOption:
		case ClassicMethod::kResetOption:
		case ClassicMethod::kSetSliderValue:
		case ClassicMethod::kSetMenuIndex:
		case ClassicMethod::kSetColorValue:
		case ClassicMethod::kSetInputText:
		case ClassicMethod::kRemapKey:
		case ClassicMethod::kSetModSettingInt:
		case ClassicMethod::kOnSettingChange:
			return true;
		default:
			return false;
		}
	}

	class IClassicScript
	{
	public:
		using Continuation = std::function<void()>;

		virtual ~IClassicScript() = default;
		// Revoke native ownership without dispatching cleanup or cancelling the VM stack.
		virtual void                                RetireExecution() {}
		virtual std::chrono::steady_clock::duration MessageWaitDuration() const { return {}; }
		virtual bool                                Dispatch(ClassicCall a_call, Continuation a_continuation) = 0;
		virtual std::vector<std::string>            ReadPages() const = 0;
		// A failed read must not erase previously published navigation.
		virtual std::optional<std::vector<std::string>> ReadNavigationPages() const { return ReadPages(); }
		virtual std::optional<ClassicPageSelection>     ReadCurrentPage() const { return std::nullopt; }
		// Native live row admission. Older read-only adapters do not expose this view.
		virtual std::optional<MCMControl>           ReadSelectionControl(std::int32_t) const { return std::nullopt; }
		virtual std::optional<ClassicPageSelection> TakePageRedirect() { return std::nullopt; }
		virtual Result<MCMPage>                     ReadPage(const ClassicPageContext& a_context) const = 0;
		virtual Result<SliderMetadata>              ReadSliderMetadata(std::uint16_t a_optionIndex) const = 0;
		virtual Result<MenuMetadata>                ReadMenuMetadata(std::uint16_t a_optionIndex) const = 0;
		virtual Result<ColorMetadata>               ReadColorMetadata(std::uint16_t a_optionIndex) const = 0;
		virtual Result<InputMetadata>               ReadInputMetadata(std::uint16_t a_optionIndex) const = 0;
		virtual void                                BeginPageCapture() {}
		virtual void                                BeginInfoCapture() {}
		virtual std::string                         ReadPageTitle() const { return {}; }
		virtual std::string                         ReadInfoText() const { return {}; }
		virtual std::optional<MCMValue>             ReadValue(MCMControlType a_type, std::uint16_t a_optionIndex) const = 0;
		virtual bool                                Matches(const SettingIdentity& a_identity, MCMControlType a_type) const = 0;
		virtual bool                                IsConfigOpen() const = 0;
		virtual bool                                IsPageReady(std::int32_t a_pageIndex) const = 0;
		virtual bool                                CanReusePage() const { return true; }
		virtual bool                                CanReuseOpeningPage() const { return false; }
		virtual std::uint64_t                       PageRevision() const { return 0; }
		virtual std::uint64_t                       PageResetRevision() const { return 0; }
		// Ordinary host-owned page traversal must not invalidate identity coverage.
		virtual std::uint64_t               IdentityRevision() const { return PageRevision(); }
		virtual void                        BeginIdentityCapture() {}
		virtual std::optional<std::int32_t> ReadOptionVariable(std::string_view, std::int32_t) const { return std::nullopt; }
	};
}
