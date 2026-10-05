#include "MCMBridge/UI/KeybindSelector.h"

#include "MCMBridge/UI/IconButton.h"
#include "MCMBridge/UI/KeymapCatalog.h"
#include "MCMBridge/UI/RichTextRenderer.h"
#include "SKSEMenuFramework.h"

namespace
{
	constexpr std::int32_t unmappedKey = -1;
	constexpr std::int32_t mouseKeyOffset = 256;

	std::optional<std::int32_t> MapGamepadKey(std::uint32_t a_key)
	{
		using Key = RE::BSWin32GamepadDevice::Key;
		switch (a_key) {
		case Key::kUp:
			return 266;
		case Key::kDown:
			return 267;
		case Key::kLeft:
			return 268;
		case Key::kRight:
			return 269;
		case Key::kStart:
			return 270;
		case Key::kBack:
			return 271;
		case Key::kLeftThumb:
			return 272;
		case Key::kRightThumb:
			return 273;
		case Key::kLeftShoulder:
			return 274;
		case Key::kRightShoulder:
			return 275;
		case Key::kA:
			return 276;
		case Key::kB:
			return 277;
		case Key::kX:
			return 278;
		case Key::kY:
			return 279;
		case Key::kLeftTrigger:
			return 280;
		case Key::kRightTrigger:
			return 281;
		default:
			return std::nullopt;
		}
	}

	std::optional<std::int32_t> MapInputKey(const RE::ButtonEvent& a_event)
	{
		const auto key = a_event.GetIDCode();
		switch (a_event.GetDevice()) {
		case RE::INPUT_DEVICE::kKeyboard:
			return key < 256 ? std::optional{ static_cast<std::int32_t>(key) } : std::nullopt;
		case RE::INPUT_DEVICE::kMouse:
			return key <= RE::BSWin32MouseDevice::Key::kWheelDown ?
			           std::optional{ mouseKeyOffset + static_cast<std::int32_t>(key) } :
			           std::nullopt;
		case RE::INPUT_DEVICE::kGamepad:
			return MapGamepadKey(key);
		default:
			return std::nullopt;
		}
	}

	class CaptureState
	{
	public:
		static CaptureState& GetSingleton()
		{
			static CaptureState singleton;
			return singleton;
		}

		bool Install()
		{
			std::scoped_lock lock(mutex);
			if (installed) {
				return true;
			}
			inputRegistration.reset(SKSEMenuFramework::AddInputEvent(OnInput));
			eventRegistration.reset(SKSEMenuFramework::AddEvent(OnFrameworkEvent, 0.0F));
			installed = inputRegistration && eventRegistration;
			return installed;
		}

		void Begin(std::string_view a_stableID)
		{
			std::scoped_lock lock(mutex);
			activeID = a_stableID;
			candidate.reset();
			renderedThisFrame = true;
		}

		void Cancel(std::string_view a_stableID)
		{
			std::scoped_lock lock(mutex);
			if (activeID == a_stableID) {
				Reset();
			}
		}

		bool IsCapturing(std::string_view a_stableID)
		{
			std::scoped_lock lock(mutex);
			if (activeID == a_stableID) {
				renderedThisFrame = true;
				return true;
			}
			return false;
		}

		std::optional<std::int32_t> Candidate(std::string_view a_stableID)
		{
			std::scoped_lock lock(mutex);
			return activeID == a_stableID ? candidate : std::nullopt;
		}

		std::optional<std::int32_t> Accept(std::string_view a_stableID)
		{
			std::scoped_lock lock(mutex);
			if (activeID != a_stableID || !candidate) {
				return std::nullopt;
			}
			const auto result = candidate;
			Reset();
			return result;
		}

		void DiscardCandidate(std::string_view a_stableID)
		{
			std::scoped_lock lock(mutex);
			if (activeID == a_stableID) {
				candidate.reset();
			}
		}

	private:
		static bool __stdcall OnInput(RE::InputEvent* a_event)
		{
			auto* mainWindow = SKSEMenuFramework::GetMainWindow();
			if (!mainWindow || !mainWindow->IsOpen.load() || !a_event) {
				return false;
			}
			const auto* button = a_event->AsButtonEvent();
			if (button && button->IsDown()) {
				GetSingleton().Capture(*button);
			}
			return false;
		}

		static void __stdcall OnFrameworkEvent(SKSEMenuFramework::Model::EventType a_eventType)
		{
			auto&            state = GetSingleton();
			std::scoped_lock lock(state.mutex);
			if (a_eventType == SKSEMenuFramework::Model::kBeforeRender) {
				state.renderedThisFrame = false;
			} else if (a_eventType == SKSEMenuFramework::Model::kAfterRender && !state.renderedThisFrame) {
				state.Reset();
			} else if (a_eventType == SKSEMenuFramework::Model::kCloseMenu) {
				state.Reset();
			}
		}

		void Capture(const RE::ButtonEvent& a_event)
		{
			std::scoped_lock lock(mutex);
			if (activeID.empty() || candidate) {
				return;
			}
			candidate = MapInputKey(a_event);
		}

		void Reset()
		{
			activeID.clear();
			candidate.reset();
			renderedThisFrame = false;
		}

		std::mutex                                            mutex;
		std::string                                           activeID;
		std::optional<std::int32_t>                           candidate;
		bool                                                  renderedThisFrame{};
		bool                                                  installed{};
		std::unique_ptr<SKSEMenuFramework::Model::InputEvent> inputRegistration;
		std::unique_ptr<SKSEMenuFramework::Model::Event>      eventRegistration;
	};

	void AlignSelector(float a_width)
	{
		const auto cursor = ImGuiMCP::GetCursorPosX();
		const auto available = ImGuiMCP::GetContentRegionAvail().x;
		ImGuiMCP::SetCursorPosX(cursor + (std::max)(0.0F, available - a_width));
	}
}

namespace MCMBridge::KeybindSelector
{
	bool Install()
	{
		return CaptureState::GetSingleton().Install();
	}

	std::optional<Edit> Render(
		std::string_view a_stableID,
		std::string_view a_label,
		std::int32_t     a_currentValue,
		bool             a_enabled,
		bool             a_allowUnmap)
	{
		auto& capture = CaptureState::GetSingleton();
		if (!a_enabled) {
			capture.Cancel(a_stableID);
		}
		const auto wasCapturing = capture.IsCapturing(a_stableID);
		const auto display = wasCapturing ? "Press a key or button..." : KeymapCatalog::Name(a_currentValue);

		if (!a_label.empty()) {
			RichTextRenderer::Render(a_label);
			ImGuiMCP::SameLine();
		}

		constexpr float cancelWidth = 62.0F;
		const auto      spacing = ImGuiMCP::GetStyle()->ItemSpacing.x;
		const auto      clearWidth = ImGuiMCP::GetFrameHeight();
		const auto      resetWidth = ImGuiMCP::GetFrameHeight();
		const auto      actionWidth = (a_allowUnmap ? clearWidth : 0.0F) + resetWidth + (wasCapturing ? cancelWidth : 0.0F);
		const auto      actionCount = 1.0F + (a_allowUnmap ? 1.0F : 0.0F) + (wasCapturing ? 1.0F : 0.0F);
		const auto      available = (std::max)(0.0F, ImGuiMCP::GetContentRegionAvail().x - spacing);
		const auto      desiredWidth = (std::max)(280.0F, available * 0.48F);
		const auto      rowWidth = (std::min)(available, desiredWidth);
		AlignSelector(rowWidth);
		const auto selectorWidth = (std::max)(1.0F, rowWidth - actionWidth - spacing * actionCount);
		const auto selectorID = std::format("{}##capture-{}", display, a_stableID);
		const auto selectorClicked = ImGuiMCP::Button(selectorID.c_str(), { selectorWidth, 0.0F });
		const auto selectorHovered = ImGuiMCP::IsItemHovered();

		ImGuiMCP::SameLine();
		static const auto resetIcon = FontAwesome::UnicodeToUtf8(0xf0e2);
		const auto        resetID = std::format("reset-{}", a_stableID);
		const auto        resetClicked = IconButton::Render(resetIcon, resetID);
		const auto        resetHovered = ImGuiMCP::IsItemHovered();
		ImGuiMCP::SetItemTooltip("Reset to default");

		bool clearClicked = false;
		bool clearHovered = false;
		if (a_allowUnmap) {
			ImGuiMCP::SameLine();
			static const auto clearIcon = FontAwesome::UnicodeToUtf8(0xf12d);
			const auto        clearID = std::format("clear-{}", a_stableID);
			clearClicked = IconButton::Render(clearIcon, clearID);
			clearHovered = ImGuiMCP::IsItemHovered();
			ImGuiMCP::SetItemTooltip("Clear binding");
		}

		bool cancelClicked = false;
		bool cancelHovered = false;
		if (wasCapturing) {
			ImGuiMCP::SameLine();
			const auto cancelID = std::format("Cancel##cancel-{}", a_stableID);
			cancelClicked = ImGuiMCP::Button(cancelID.c_str(), { cancelWidth, 0.0F });
			cancelHovered = ImGuiMCP::IsItemHovered();
		}

		if (clearClicked) {
			capture.Cancel(a_stableID);
			return Edit{ Action::kSetValue, unmappedKey };
		}
		if (resetClicked) {
			capture.Cancel(a_stableID);
			return Edit{ Action::kReset, a_currentValue };
		}
		if (wasCapturing && (selectorClicked || cancelClicked)) {
			capture.Cancel(a_stableID);
			return std::nullopt;
		}
		if (!wasCapturing && selectorClicked) {
			capture.Begin(a_stableID);
			return std::nullopt;
		}

		const auto candidate = capture.Candidate(a_stableID);
		const auto ownButtonHovered = selectorHovered || resetHovered || clearHovered || cancelHovered;
		if (candidate == mouseKeyOffset && ownButtonHovered) {
			capture.DiscardCandidate(a_stableID);
			return std::nullopt;
		}
		if (const auto accepted = capture.Accept(a_stableID)) {
			return Edit{ Action::kSetValue, *accepted };
		}
		return std::nullopt;
	}
}
