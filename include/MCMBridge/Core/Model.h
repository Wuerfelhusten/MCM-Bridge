#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace MCMBridge
{
	class WriteTiming;

	enum class MCMBackendKind
	{
		kClassicSkyUI,
		kMCMHelper
	};

	enum class MCMControlType
	{
		kEmpty,
		kHeader,
		kText,
		kToggle,
		kSlider,
		kMenu,
		kStepper,
		kColor,
		kKeymap,
		kInput,
		kUnknown
	};

	enum class IdentityConfidence
	{
		kLow,
		kHigh
	};

	enum class MetadataAvailability
	{
		kAvailable,
		kMissing,
		kDynamic
	};

	enum class WriteCapability
	{
		kReadOnly,
		kWritable,
		kDisabled,
		kMissingOptions,
		kUnsupported
	};

	enum class WriteStatus
	{
		kIdle,
		kPending,
		kApplied,
		kRejected,
		kTimedOut,
		kStaleSnapshot,
		kUnchanged
	};

	enum class WriteIntent
	{
		kSetValue,
		kActivate,
		kReset
	};

	enum class ValueSourceKind
	{
		kNone,
		kModSetting,
		kProperty,
		kGlobal,
		kDerived
	};

	enum class DiagnosticSeverity
	{
		kInfo,
		kWarning,
		kError
	};

	using MCMValue = std::variant<std::monostate, bool, std::int32_t, float, std::string, std::uint32_t>;

	struct SettingIdentity
	{
		std::string        stableID;
		MCMBackendKind     backend{ MCMBackendKind::kClassicSkyUI };
		std::string        ownerPlugin;
		std::uint32_t      questFormID{};
		std::string        scriptName;
		std::string        stateName;
		std::string        pageKey;
		std::string        explicitID;
		std::int32_t       pageIndex{};
		std::uint16_t      optionIndex{};
		IdentityConfidence confidence{ IdentityConfidence::kLow };

		bool operator==(const SettingIdentity&) const = default;
	};

	struct SliderMetadata
	{
		float                start{};
		float                defaultValue{};
		float                minimum{};
		float                maximum{};
		float                step{ 1.0F };
		std::string          format;
		MetadataAvailability availability{ MetadataAvailability::kMissing };
		bool                 operator==(const SliderMetadata&) const = default;
	};

	struct MenuMetadata
	{
		std::vector<std::string> options;
		std::vector<std::string> shortNames;
		std::vector<std::string> displayOptions;
		std::vector<std::string> displayShortNames;
		std::int32_t             selectedIndex{ -1 };
		std::int32_t             defaultIndex{ -1 };
		MetadataAvailability     availability{ MetadataAvailability::kMissing };
		bool                     operator==(const MenuMetadata&) const = default;
	};

	struct ColorMetadata
	{
		std::uint32_t        start{};
		std::uint32_t        defaultValue{};
		MetadataAvailability availability{ MetadataAvailability::kMissing };
		bool                 operator==(const ColorMetadata&) const = default;
	};

	struct InputMetadata
	{
		std::string          startText;
		MetadataAvailability availability{ MetadataAvailability::kMissing };
		bool                 operator==(const InputMetadata&) const = default;
	};

	struct ControlLayout
	{
		std::int32_t position{ -1 };
		std::int32_t column{};
		bool         operator==(const ControlLayout&) const = default;
	};

	struct CustomContentMetadata
	{
		std::string source;
		float       x{};
		float       y{};
		bool        operator==(const CustomContentMetadata&) const = default;
	};

	struct ValueSource
	{
		ValueSourceKind kind{ ValueSourceKind::kNone };
		std::string     sourceType;
		std::string     settingID;
		std::string     sourceForm;
		std::string     scriptName;
		std::string     propertyName;
		std::uint32_t   formID{};
		bool            operator==(const ValueSource&) const = default;
	};

	struct ActionMetadata
	{
		std::string           type;
		std::string           functionName;
		std::string           form;
		std::string           scriptName;
		std::string           command;
		std::vector<MCMValue> parameters;
		bool                  operator==(const ActionMetadata&) const = default;
	};

	struct ConditionMetadata
	{
		std::string expression;
		std::string behavior;
		bool        operator==(const ConditionMetadata&) const = default;
	};

	struct MCMControl
	{
		SettingIdentity                  identity;
		MCMControlType                   type{ MCMControlType::kUnknown };
		std::string                      label;
		std::string                      rawLabel;
		std::string                      help;
		std::string                      displayValue;
		bool                             disabled{};
		bool                             hidden{};
		MCMValue                         value;
		std::optional<MCMValue>          defaultValue;
		std::optional<SliderMetadata>    slider;
		std::optional<MenuMetadata>      menu;
		std::optional<ColorMetadata>     color;
		std::optional<InputMetadata>     input;
		ValueSource                      source;
		std::optional<ActionMetadata>    action;
		std::optional<ConditionMetadata> condition;
		std::uint32_t                    groupControl{};
		bool                             ignoreConflicts{};
		bool                             allowUnmap{};
		ControlLayout                    layout;
		WriteCapability                  writeCapability{ WriteCapability::kReadOnly };
		WriteStatus                      writeStatus{ WriteStatus::kIdle };
		bool                             metadataCurrent{ true };
		bool                             operator==(const MCMControl&) const = default;
	};

	struct MCMPage
	{
		std::string                          stableID;
		std::string                          rawName;
		std::string                          displayName;
		std::string                          title;
		std::int32_t                         index{};
		std::optional<CustomContentMetadata> customContent;
		std::vector<MCMControl>              controls;
		// A registration-only entry, not a page confirmed by a mod callback.
		bool openingPlaceholder{};
		bool operator==(const MCMPage&) const = default;
	};

	struct MCMMod
	{
		std::string              stableID;
		std::string              displayName;
		MCMBackendKind           backend{ MCMBackendKind::kClassicSkyUI };
		std::string              ownerPlugin;
		std::uint32_t            questFormID{};
		std::string              scriptName;
		std::string              interopID;
		bool                     pageScopedState{};
		std::uint32_t            minimumMCMHelperVersion{};
		std::vector<std::string> pluginRequirements;
		std::vector<MCMPage>     pages;
	};

	struct Diagnostic
	{
		DiagnosticSeverity severity{ DiagnosticSeverity::kInfo };
		std::string        sourceID;
		std::string        message;
	};

	struct MCMSnapshot
	{
		std::uint64_t                         generation{};
		std::chrono::steady_clock::time_point createdAt;
		bool                                  refreshing{};
		std::vector<MCMMod>                   mods;
		std::vector<Diagnostic>               diagnostics;
	};

	struct WriteCommand
	{
		std::uint64_t                      snapshotGeneration{};
		std::string                        settingID;
		SettingIdentity                    expectedIdentity;
		MCMControlType                     expectedType{ MCMControlType::kUnknown };
		MCMValue                           expectedValue;
		MCMValue                           desiredValue;
		WriteIntent                        intent{ WriteIntent::kSetValue };
		std::string                        conflictControl;
		std::string                        conflictName;
		std::shared_ptr<WriteTiming>       timing;
		std::uint64_t                      pauseTicket{};
		std::shared_ptr<const MCMSnapshot> recordingSnapshot;
		std::uint64_t                      recordingID{};
		std::uint64_t                      recordingSession{};
	};
}
