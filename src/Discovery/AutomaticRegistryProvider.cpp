#include "MCMBridge/Discovery/AutomaticRegistryProvider.h"
#include "MCMBridge/Papyrus/HelperNativeUI.h"
#include "MCMBridge/Papyrus/NativeFacade.h"
#include "MCMBridge/Papyrus/NativeHostUI.h"

namespace
{
	// Bootstrap only: registration remains owned by the native registry.
	RE::BSTSmartPointer<RE::BSScript::Object> ReadManager()
	{
		auto* quest = RE::TESForm::LookupByEditorID<RE::TESQuest>("SKI_ConfigManagerInstance");
		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		auto* policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
		if (!quest || !vm || !policy) {
			return {};
		}

		const auto                                handle = policy->GetHandleForObject(quest->GetFormType(), quest);
		RE::BSTSmartPointer<RE::BSScript::Object> manager;
		if (handle == policy->EmptyHandle() || !vm->FindBoundObject(handle, "SKI_ConfigManager", manager)) {
			return {};
		}
		return manager;
	}
}

namespace MCMBridge
{
	Result<bool> AutomaticRegistryProvider::PrepareNative(std::uint64_t a_session)
	{
		auto manager = ReadManager();
		if (!manager)
			return std::unexpected(BridgeError{ BridgeErrorCode::kBusy, "Waiting for the native manager instance" });
		std::string reason;
		if (!HasNativeFacadeContract(manager, true, &reason))
			return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "Native manager admission failed: " + reason });
		return ActivateNative(std::move(manager), a_session);
	}

	Result<std::vector<MCMDescriptor>> AutomaticRegistryProvider::Read()
	{
		auto live = ReadLive();
		if (!live) {
			return std::unexpected(live.error());
		}
		std::vector<MCMDescriptor> descriptors;
		descriptors.reserve(live->size());
		for (const auto& entry : *live) {
			descriptors.push_back(entry.descriptor);
		}
		return descriptors;
	}

	bool AutomaticRegistryProvider::IsBusy() const
	{
		return native.IsBusy();
	}

	Result<std::vector<LiveMCM>> AutomaticRegistryProvider::ReadLive()
	{
		LogSelection(native.Name());
		return native.ReadLive();
	}

	bool AutomaticRegistryProvider::IsAvailable() const
	{
		return native.IsAvailable();
	}

	std::string_view AutomaticRegistryProvider::Name() const
	{
		return native.Name();
	}

	void AutomaticRegistryProvider::Reset(bool a_invalidate)
	{
		if (a_invalidate)
			native.Reset();
		selectedName.clear();
	}

	Result<bool> AutomaticRegistryProvider::ActivateNative(RE::BSTSmartPointer<RE::BSScript::Object> a_manager, std::uint64_t a_session)
	{
		if (!IsHelperHostReady())
			return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "Native host requires compatible MCM Helper capture" });
		if (!NativeHostUI::IsReady())
			return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "Native host requires scoped UI cursor bindings" });
		return native.Bootstrap(std::move(a_manager), a_session);
	}

	void AutomaticRegistryProvider::LogSelection(std::string_view a_name)
	{
		if (selectedName == a_name) {
			return;
		}
		selectedName = a_name;
		SKSE::log::info("Selected {} MCM registry provider", a_name);
	}
}
