#pragma once

#include "MCMBridge/Core/HostPageState.h"
#include "MCMBridge/Papyrus/IClassicScript.h"
#include "MCMBridge/Papyrus/NativeHostCallback.h"

#include "RE/Skyrim.h"
#include <unordered_set>

namespace MCMBridge
{
	// Owns the bound script handle. Dispatch and buffer reads belong to the game thread.
	class MCMScript final : public IClassicScript
	{
	public:
		explicit MCMScript(RE::BSTSmartPointer<RE::BSScript::Object> a_script);

		bool                                    Dispatch(ClassicCall a_call, Continuation a_continuation) override;
		std::vector<std::string>                ReadPages() const override;
		std::optional<std::vector<std::string>> ReadNavigationPages() const override;
		std::optional<ClassicPageSelection>     ReadCurrentPage() const override;
		Result<MCMPage>                         ReadPage(const ClassicPageContext& a_context) const override;
		Result<SliderMetadata>                  ReadSliderMetadata(std::uint16_t a_optionIndex) const override;
		Result<MenuMetadata>                    ReadMenuMetadata(std::uint16_t a_optionIndex) const override;
		Result<ColorMetadata>                   ReadColorMetadata(std::uint16_t a_optionIndex) const override;
		Result<InputMetadata>                   ReadInputMetadata(std::uint16_t a_optionIndex) const override;
		void                                    BeginPageCapture() override;
		void                                    BeginInfoCapture() override;
		std::string                             ReadPageTitle() const override;
		std::string                             ReadInfoText() const override;
		std::optional<MCMValue>                 ReadValue(MCMControlType a_type, std::uint16_t a_optionIndex) const override;
		bool                                    Matches(const SettingIdentity& a_identity, MCMControlType a_type) const override;
		bool                                    IsConfigOpen() const override;
		bool                                    IsPageReady(std::int32_t a_pageIndex) const override;
		bool                                    CanReusePage() const override;
		bool                                    CanReuseOpeningPage() const override;
		std::uint64_t                           PageRevision() const override;
		std::optional<std::int32_t>             ReadOptionVariable(std::string_view a_name, std::int32_t a_pageIndex) const override;

	private:
		bool Call(
			std::string_view                  a_functionName,
			RE::BSScript::IFunctionArguments* a_arguments,
			SKSE::TaskInterface::TaskFn       a_continuation) const;
		bool                                     IsMenuReady(std::uint16_t a_optionIndex) const;
		const RE::BSScript::Variable*            FindVariable(std::string_view a_name) const;
		RE::BSTSmartPointer<RE::BSScript::Array> ReadArray(std::string_view a_name) const;
		std::optional<float>                     ReadNumber(std::string_view a_name, std::size_t a_index) const;
		std::optional<std::string>               ReadString(std::string_view a_name, std::size_t a_index) const;
		std::optional<std::string>               ReadScalarString(std::string_view a_name) const;
		std::optional<std::int32_t>              ReadInteger(std::string_view a_name) const;

		RE::BSTSmartPointer<RE::BSScript::Object> script;
		std::uint64_t                             capturedPageRevision{};
		struct HostObservationState
		{
			std::uint64_t         operation{};
			NativeHostObservation last;
			bool                  completed{};
			HostPageState         pageState;
			bool                  pendingPageChanges{};
		};
		std::shared_ptr<HostObservationState> hostObservation{ std::make_shared<HostObservationState>() };
	};
}
