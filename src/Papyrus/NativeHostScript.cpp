#include "MCMBridge/Papyrus/NativeHostScript.h"
#include "MCMBridge/Core/HelperMenuCapture.h"

#include "MCMBridge/Papyrus/MCMScript.h"
#include "MCMBridge/Papyrus/NativeFacade.h"
#include "MCMBridge/Papyrus/NativeHostBinding.h"

namespace
{
	using namespace MCMBridge;

	class NativeScript final : public IClassicScript
	{
	public:
		NativeScript(std::unique_ptr<NativeHostBinding> a_binding, RE::BSTSmartPointer<RE::BSScript::Object> a_script) :
			binding(std::move(a_binding)), script(a_script), mirror(std::move(a_script)) {}

		bool Dispatch(ClassicCall a_call, Continuation a_continuation) override
		{
			if (a_call.method == ClassicMethod::kOpenConfig || a_call.method == ClassicMethod::kSetPage) {
				const auto page = Page();
				resetRevision = page ? page->resetRevision : 0;
			}
			return binding->Dispatch(std::move(a_call), std::move(a_continuation));
		}
		void RetireExecution() override
		{
			const auto identity = NativeFacadeSession().ReadIdentity(binding->Token());
			binding->Invalidate();
			if (identity)
				SKSE::log::warn("Native MCM execution retired: mod={} host access revoked; Papyrus may still finish, no cleanup dispatched", identity->modID);
		}
		std::chrono::steady_clock::duration MessageWaitDuration() const override
		{
			return NativeFacadeSession().MessageWaitDuration(binding->Token());
		}
		std::vector<std::string> ReadPages() const override
		{
			const auto pages = ReadNavigationPages();
			return pages.value_or(std::vector<std::string>{});
		}
		std::optional<ClassicPageSelection>     TakePageRedirect() override { return binding->TakePageRedirect(); }
		std::optional<std::vector<std::string>> ReadNavigationPages() const override
		{
			if (binding->HasPage()) {
				if (auto pages = NativeFacadeSession().ReadNavigation(binding->Token()))
					return pages;
			}
			return mirror.ReadNavigationPages();
		}
		std::optional<ClassicPageSelection> ReadCurrentPage() const override
		{
			return binding->HasPage() ? mirror.ReadCurrentPage() : std::nullopt;
		}
		std::optional<MCMControl> ReadSelectionControl(std::int32_t a_index) const override
		{
			const auto page = Page();
			if (!page || a_index < 0 || !IsPageReady(page->index))
				return std::nullopt;
			const auto  slot = static_cast<std::size_t>(a_index);
			const auto& buffers = page->buffers;
			if (slot >= buffers.optionFlags.size() || slot >= buffers.labels.size() ||
				slot >= buffers.stringValues.size() || slot >= buffers.stateNames.size())
				return std::nullopt;
			const auto flags = buffers.optionFlags[slot];
			const auto type = flags & 0xFF;
			MCMControl control;
			control.type = type == 2 ? MCMControlType::kText : type == 3 ? MCMControlType::kToggle :
			                                                               MCMControlType::kUnknown;
			control.rawLabel = buffers.labels[slot];
			control.identity.stateName = buffers.stateNames[slot];
			control.disabled = (flags & 0x100) != 0;
			control.hidden = (flags & 0x200) != 0;
			control.value = buffers.stringValues[slot];
			return control;
		}
		Result<MCMPage> ReadPage(const ClassicPageContext& a_context) const override
		{
			const auto page = Page();
			if (!page || !IsConfigOpen() || page->modID != a_context.modID || page->index != a_context.pageIndex || page->page != a_context.pageName)
				return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Native host page is not current" });
			auto result = ParseClassicPage(a_context, page->buffers);
			if (result) {
				result->title = page->title;
				if (!page->customSource.empty())
					result->customContent = CustomContentMetadata{ .source = page->customSource, .x = page->customX, .y = page->customY };
			}
			return result;
		}
		Result<SliderMetadata> ReadSliderMetadata(std::uint16_t a_index) const override
		{
			const auto dialog = Dialog(a_index, 4);
			if (!dialog)
				return std::unexpected(dialog.error());
			const auto& values = dialog->slider;
			return SliderMetadata{ .start = values[0], .defaultValue = values[1], .minimum = values[2], .maximum = values[3], .step = values[4], .availability = MetadataAvailability::kAvailable };
		}
		Result<MenuMetadata> ReadMenuMetadata(std::uint16_t a_index) const override
		{
			const auto dialog = Dialog(a_index, 5);
			if (!dialog)
				return std::unexpected(dialog.error());
			const auto selected = MenuIndex(0);
			const auto defaultIndex = MenuIndex(1);
			if (!selected || !defaultIndex)
				return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Native menu indices are missing or invalid" });
			return MenuMetadata{ .options = dialog->options, .selectedIndex = *selected, .defaultIndex = *defaultIndex, .availability = dialog->options.empty() ? MetadataAvailability::kMissing : MetadataAvailability::kAvailable };
		}
		Result<ColorMetadata> ReadColorMetadata(std::uint16_t a_index) const override
		{
			const auto dialog = Dialog(a_index, 6);
			if (!dialog)
				return std::unexpected(dialog.error());
			return ColorMetadata{ .start = static_cast<std::uint32_t>(dialog->color[0]),
				.defaultValue = static_cast<std::uint32_t>(dialog->color[1]),
				.availability = MetadataAvailability::kAvailable };
		}
		Result<InputMetadata> ReadInputMetadata(std::uint16_t a_index) const override
		{
			const auto dialog = Dialog(a_index, 8);
			if (!dialog)
				return std::unexpected(dialog.error());
			return InputMetadata{ .startText = dialog->input, .availability = MetadataAvailability::kAvailable };
		}
		std::optional<MCMValue> ReadValue(MCMControlType a_type, std::uint16_t a_index) const override
		{
			return IsConfigOpen() ? mirror.ReadValue(a_type, a_index) : std::nullopt;
		}
		bool Matches(const SettingIdentity& a_identity, MCMControlType a_type) const override
		{
			const auto page = ReadCurrentPage();
			return page && page->index == a_identity.pageIndex && page->name == a_identity.pageKey && mirror.Matches(a_identity, a_type);
		}
		bool IsConfigOpen() const override { return binding->HasPage() && mirror.IsConfigOpen(); }
		bool IsPageReady(std::int32_t a_index) const override { return IsConfigOpen() && mirror.IsPageReady(a_index); }
		bool CanReusePage() const override
		{
			const auto page = Page();
			return page && IsConfigOpen() && page->resetRevision == resetRevision;
		}
		bool          CanReuseOpeningPage() const override { return CanReusePage(); }
		std::uint64_t IdentityRevision() const override { return NativeFacadeSession().IdentityRevision(); }
		void          BeginIdentityCapture() override { NativeFacadeSession().BeginIdentityCapture(binding->Token()); }
		std::uint64_t PageRevision() const override
		{
			const auto page = Page();
			return page ? page->valueRevision : 0;
		}
		std::uint64_t PageResetRevision() const override
		{
			const auto page = Page();
			return page ? page->resetRevision : 0;
		}
		std::string ReadPageTitle() const override
		{
			const auto page = Page();
			return page ? page->title : std::string{};
		}
		std::string ReadInfoText() const override
		{
			const auto page = Page();
			return page ? page->info : std::string{};
		}
		std::optional<std::int32_t> ReadOptionVariable(std::string_view a_name, std::int32_t a_pageIndex) const override
		{
			return IsPageReady(a_pageIndex) ? mirror.ReadOptionVariable(a_name, a_pageIndex) : std::nullopt;
		}

	private:
		std::optional<std::int32_t> MenuIndex(std::uint32_t a_index) const
		{
			const auto* variable = script->GetVariable("_menuParams");
			const auto  values = variable && variable->IsArray() ? variable->GetArray() : nullptr;
			if (!values || values->size() != 2 || a_index >= values->size())
				return std::nullopt;
			const auto& value = (*values)[a_index];
			if (value.IsInt())
				return ConvertMenuDialogIndex(value.GetSInt());
			if (value.IsFloat())
				return ConvertMenuDialogIndex(value.GetFloat());
			return std::nullopt;
		}
		bool                                  Active() const { return NativeFacadeSession().IsActive(binding->Token()); }
		std::shared_ptr<const NativeHostPage> Page() const { return binding->HasPage() ? NativeFacadeSession().Read() : nullptr; }
		Result<NativeHostDialog>              Dialog(std::uint16_t a_index, std::int32_t a_type) const
		{
			const auto page = ReadCurrentPage();
			const auto dialog = NativeFacadeSession().ReadDialog(binding->Token());
			if (!page || !dialog || dialog->type != a_type || dialog->optionID != (page->index + 1) * 256 + a_index)
				return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Native dialog metadata is not current" });
			return *dialog;
		}
		std::unique_ptr<NativeHostBinding>        binding;
		RE::BSTSmartPointer<RE::BSScript::Object> script;
		// Reuse only engine reads. The legacy adapter never dispatches or captures UI here.
		MCMScript     mirror;
		std::uint64_t resetRevision{};
	};
}

namespace MCMBridge
{
	Result<std::shared_ptr<IClassicScript>> CreateNativeHostScript(std::uint64_t a_session,
		std::string a_modID, RE::BSTSmartPointer<RE::BSScript::Object> a_script)
	{
		auto binding = NativeHostBinding::Create(a_session, std::move(a_modID), a_script);
		if (!binding)
			return std::unexpected(binding.error());
		return std::make_shared<NativeScript>(std::move(*binding), std::move(a_script));
	}
}
