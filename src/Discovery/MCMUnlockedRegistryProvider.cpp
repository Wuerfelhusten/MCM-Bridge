#include "MCMBridge/Discovery/MCMUnlockedRegistryProvider.h"
#include "MCMBridge/Discovery/LiveMCMFactory.h"
#include "MCMBridge/Discovery/UnlockedNativeRegistry.h"
#include <unordered_set>

namespace MCMBridge
{
	Result<std::vector<MCMDescriptor>> MCMUnlockedRegistryProvider::Read()
	{
		auto live = ReadLive();
		if (!live)
			return std::unexpected(live.error());
		std::vector<MCMDescriptor> result;
		for (const auto& entry : *live) result.push_back(entry.descriptor);
		return result;
	}

	void MCMUnlockedRegistryProvider::Reset()
	{
		if (query)
			query->Cancel();
		query.reset();
	}

	Result<std::vector<LiveMCM>> MCMUnlockedRegistryProvider::ReadLive()
	{
		if (!query) {
			query = CreateUnlockedRegistryQuery();
			query->SetCompletion(completion);
			query->Start();
		}
		auto records = query->Poll();
		if (!records && records.error().code == BridgeErrorCode::kBusy)
			return std::unexpected(records.error());
		query.reset();
		if (!records)
			return std::unexpected(records.error());
		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		auto* policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
		if (!vm || !policy)
			return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "Papyrus VM unavailable" });
		std::vector<LiveMCM>            result;
		std::unordered_set<std::string> identities;
		for (const auto& record : *records) {
			RE::BSTSmartPointer<RE::BSScript::Object> marker;
			if (!vm->FindBoundObject(record.marker, "MCMUnlockedMarkerScript", marker) || !marker) {
				return std::unexpected(BridgeError{ BridgeErrorCode::kStaleSnapshot, "MCM Unlocked marker is unavailable" });
			}
			const auto* instance = marker->GetProperty("InstanceScript");
			if (!instance || !instance->IsObject())
				instance = marker->GetVariable("::InstanceScript_var");
			if (!instance || !instance->IsObject() || !instance->GetObject()) {
				return std::unexpected(BridgeError{ BridgeErrorCode::kStaleSnapshot, "MCM Unlocked marker is not initialized" });
			}
			auto entry = CreateLiveMCM(instance->GetObject(), -1);
			if (!entry)
				return std::unexpected(entry.error());
			if (!identities.insert(entry->descriptor.stableID).second) {
				return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Duplicate MCM Unlocked script identity" });
			}
			entry->registryID = record.id;
			if (record.displayName != record.id)
				entry->registryDisplayName = record.displayName;
			result.push_back(std::move(*entry));
		}
		SKSE::log::info("MCM Unlocked registry verified {} active entries", result.size());
		return result;
	}

	bool MCMUnlockedRegistryProvider::IsBusy() const
	{
		auto        manager = ReadManager();
		const auto* active = manager ? manager->GetVariable("_activeConfig") : nullptr;
		return active && active->IsObject() && active->GetObject();
	}

	bool MCMUnlockedRegistryProvider::IsAvailable() const
	{
		auto* data = RE::TESDataHandler::GetSingleton();
		return data && data->LookupForm<RE::TESObjectACTI>(0x800, "MCM Unlocked.esp") &&
		       data->LookupForm<RE::TESObjectCELL>(0x801, "MCM Unlocked.esp");
	}

	std::string_view MCMUnlockedRegistryProvider::Name() const { return "MCM Unlocked"; }

	RE::BSTSmartPointer<RE::BSScript::Object> MCMUnlockedRegistryProvider::ReadManager()
	{
		auto* quest = RE::TESForm::LookupByEditorID<RE::TESQuest>("SKI_ConfigManagerInstance");
		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		auto* policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
		if (!quest || !vm || !policy)
			return {};
		const auto                                handle = policy->GetHandleForObject(quest->GetFormType(), quest);
		RE::BSTSmartPointer<RE::BSScript::Object> manager;
		if (handle == policy->EmptyHandle() || !vm->FindBoundObject(handle, "SKI_ConfigManager", manager))
			return {};
		return manager;
	}
}
