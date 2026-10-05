#include "MCMBridge/Plugin/Plugin.h"

#include "MCMBridge/Discovery/SkyUIRegistryEvents.h"
#include "MCMBridge/Framework/FrameworkApi.h"
#include "MCMBridge/Papyrus/HelperControlCapture.h"
#include "MCMBridge/Papyrus/HelperCustomCapture.h"
#include "MCMBridge/Papyrus/HelperMessageCapture.h"
#include "MCMBridge/Papyrus/HelperNativeUI.h"
#include "MCMBridge/Papyrus/NativeFacade.h"
#include "MCMBridge/Papyrus/NativeHostPreflight.h"
#include "MCMBridge/Papyrus/UIInvokeStringArrayBridge.h"
#include "MCMBridge/Plugin/BridgeController.h"
#include "MCMBridge/Plugin/BridgeSettingsService.h"
#include "MCMBridge/Plugin/GamePauseMenu.h"
#include "MCMBridge/Plugin/GameSessionEvents.h"
#include "MCMBridge/Plugin/JournalRedirect.h"
#include "MCMBridge/Plugin/StartupCompatibility.h"
#include "MCMBridge/Plugin/WritePauseService.h"

#include <Windows.h>

namespace MCMBridge::Plugin
{
	bool Activate();
}

namespace
{
	bool          incompatiblePluginDetected{};
	bool          functionalReady{};
	std::uint32_t startupRuntime{};
	bool          recorderCheckDeferred{};

	bool HasIncompatiblePlugin()
	{
		return GetModuleHandleW(L"MCMMenuRedone.dll") != nullptr;
	}

	void OnMessage(SKSE::MessagingInterface::Message* a_message)
	{
#ifdef MCM_BRIDGE_NATIVE_HOST_PREFLIGHT
		if (a_message->type == SKSE::MessagingInterface::kDataLoaded) {
			SKSE::log::info("Native host preflight: DataLoaded received; incompatible={} ready={}", incompatiblePluginDetected, functionalReady);
			spdlog::default_logger()->flush();
		}
#endif
		if (incompatiblePluginDetected)
			return;
		using Message = SKSE::MessagingInterface;
		if (a_message->type == Message::kPostLoad) {
			const auto unlocked = MCMBridge::IsUnlockedSupportedAtStartup();
			if (!unlocked)
				SKSE::log::error("MCM Unlocked startup check failed: {}", unlocked.error().message);
			const auto recorder = MCMBridge::IsRecorderEnabledAtStartup(startupRuntime);
			recorderCheckDeferred = !recorder.has_value();
			const bool redone = HasIncompatiblePlugin();
			const bool seeded = GetModuleHandleW(L"MCM_Super_SEEDED.dll") != nullptr;
			const bool menuMaid = GetModuleHandleW(L"MenuMaid2.dll") != nullptr;
			if (!recorder)
				SKSE::log::warn("Early Recorder compatibility is unknown: {}; deferring to loaded plugins", recorder.error().message);
			else
				SKSE::log::info("Early plugins.txt check: Recorder enabled={}", *recorder);
			if (redone || seeded || menuMaid || recorder.value_or(false) || !unlocked.value_or(false)) {
				incompatiblePluginDetected = true;
				SKSE::log::critical("Startup incompatibility: Redone={} Recorder={} SEEDED={} Unlocked={} MenuMaid2={}; MCM Bridge is completely disabled", redone, recorder.value_or(false), seeded, !unlocked.value_or(false), menuMaid);
				MCMBridge::ShowStartupIncompatibility(redone, recorder.value_or(false), seeded, !unlocked.value_or(false), menuMaid);
				return;
			}
			if (!MCMBridge::InstallHelperHostCapture())
				SKSE::log::error("Native host admission blocked: required Helper capture is unavailable");
			return;
		}
		if (a_message->type == Message::kDataLoaded && !functionalReady) {
			if (recorderCheckDeferred) {
				auto* data = RE::TESDataHandler::GetSingleton();
				if (!data) {
					SKSE::log::error("Deferred Recorder check failed: data handler unavailable; Bridge remains inactive");
					return;
				}
				recorderCheckDeferred = false;
				const bool recorder = data->LookupLoadedModByName("McmRecorder.esp") || data->LookupLoadedLightModByName("McmRecorder.esp");
				SKSE::log::info("Deferred Recorder check: loaded={}", recorder);
				if (recorder) {
					incompatiblePluginDetected = true;
					SKSE::log::critical("McmRecorder.esp is active; MCM Bridge is completely disabled");
					if (auto* tasks = SKSE::GetTaskInterface()) {
						tasks->AddTask([] { MCMBridge::ShowStartupIncompatibility(false, true, false); });
					} else {
						SKSE::log::error("Cannot queue incompatibility dialog: game task interface unavailable");
					}
					return;
				}
			}
			const auto scripts = MCMBridge::FindIncompatibleHostScripts();
			if (!scripts.empty()) {
				incompatiblePluginDetected = true;
				SKSE::log::critical("Host script installation check failed; MCM Bridge remains inactive");
				if (auto* tasks = SKSE::GetTaskInterface())
					tasks->AddTask([scripts] { MCMBridge::ShowStartupIncompatibility(false, false, false, false, false, scripts); });
				return;
			}
			functionalReady = MCMBridge::Plugin::Activate();
		}
		if (!functionalReady)
			return;
		auto& controller = MCMBridge::BridgeController::GetSingleton();
		switch (a_message->type) {
		case Message::kDataLoaded:
#ifdef MCM_BRIDGE_NATIVE_HOST_PREFLIGHT
			if (auto* tasks = SKSE::GetTaskInterface()) {
				SKSE::log::info("Native host preflight: queueing game task");
				spdlog::default_logger()->flush();
				tasks->AddTask([] { MCMBridge::RunNativeHostPreflight(); });
			} else {
				SKSE::log::error("Native host preflight: cannot queue game task; task interface unavailable");
				spdlog::default_logger()->flush();
			}
#endif
			if (!MCMBridge::GameSessionEvents::Install()) {
				SKSE::log::error("Could not install the game session event listener");
			}
			if (!MCMBridge::GamePauseMenu::Install()) {
				SKSE::log::error("Could not register the setting change pause menu");
			}
			// The native manager binds after DataLoaded, so Journal interception
			// cannot depend on the registry already being available here.
			if (!MCMBridge::JournalRedirect::Install()) {
				SKSE::log::error("Could not install the Journal MCM redirect");
			}
			if (controller.IsSkyUIAvailable()) {
				SKSE::log::info("SkyUI config manager is available");
				MCMBridge::SkyUIRegistryEvents::GetSingleton().Install();
			} else {
				SKSE::log::info("Native MCM registry is not bound at DataLoaded; Journal redirect is already installed");
			}
			controller.RequestRefresh(true);
			break;
		case Message::kPreLoadGame:
			MCMBridge::GameSessionEvents::BeginLoad();
			controller.StartSession("Game load started", false);
			break;
		case Message::kPostLoadGame:
			MCMBridge::GameSessionEvents::GameStarted();
			controller.StartSession("Loaded game changed");
			break;
		case Message::kNewGame:
			MCMBridge::GameSessionEvents::GameStarted();
			controller.StartSession("New game started");
			break;
		default:
			break;
		}
	}
}

namespace MCMBridge::Plugin
{
	bool Activate()
	{
		if (!IsNativeFacadeReady()) {
			SKSE::log::critical("Native facade bindings are incomplete; MCM Bridge remains inactive");
			return false;
		}
		if (!RegisterNativeManager(RE::BSScript::Internal::VirtualMachine::GetSingleton())) {
			SKSE::log::critical("Native manager transport is incomplete; MCM Bridge remains inactive");
			return false;
		}
		BridgeSettingsService::GetSingleton().Load();
		WritePauseService::GetSingleton().LoadSettings();
		if (!UIInvokeStringArrayBridge::Register(RE::BSScript::Internal::VirtualMachine::GetSingleton())) {
			SKSE::log::critical("Could not register the Papyrus UI bridge");
			return false;
		}
		if (!FrameworkApi::GetSingleton().BindAndRegister()) {
			SKSE::log::critical("Could not bind Menu Framework; MCM Bridge remains inactive");
			return false;
		}
		return true;
	}

	bool Initialize(std::uint32_t a_packedRuntime)
	{
#ifdef MCM_BRIDGE_NATIVE_HOST_PREFLIGHT
		SKSE::log::info("Native host preflight: enabled; waiting for DataLoaded");
		spdlog::default_logger()->flush();
#endif
		startupRuntime = a_packedRuntime;
		const auto* papyrus = SKSE::GetPapyrusInterface();
		if (!papyrus || !papyrus->Register([](RE::BSScript::IVirtualMachine* a_vm) {
				if (incompatiblePluginDetected)
					return true;
				return MCMBridge::RegisterNativeFacade(a_vm);
			})) {
			SKSE::log::critical("Could not register the native MCM facade protocol");
			return false;
		}
		const auto* messaging = SKSE::GetMessagingInterface();
		if (!messaging || !messaging->RegisterListener(OnMessage)) {
			SKSE::log::critical("Could not register the SKSE message listener");
			return false;
		}
		return true;
	}
}
