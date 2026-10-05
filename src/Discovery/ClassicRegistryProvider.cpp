#include "MCMBridge/Discovery/ClassicRegistryProvider.h"

#include "MCMBridge/Discovery/LiveMCMFactory.h"

namespace MCMBridge
{
	Result<std::vector<MCMDescriptor>> ClassicRegistryProvider::Read()
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

	Result<std::vector<LiveMCM>> ClassicRegistryProvider::ReadLive()
	{
		auto manager = ReadManager();
		if (!manager) {
			return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "SkyUI config manager is unavailable" });
		}

		const auto* value = manager->GetVariable(RE::BSFixedString("_modConfigs"));
		if (!value || !value->IsObjectArray()) {
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "SkyUI _modConfigs is unavailable" });
		}
		auto values = value->GetArray();
		if (!values) {
			return std::vector<LiveMCM>{};
		}

		std::vector<LiveMCM> result;
		result.reserve(values->size());
		for (std::uint32_t configIndex = 0; configIndex < values->size(); ++configIndex) {
			const auto& item = (*values)[configIndex];
			if (!item.IsObject()) {
				continue;
			}
			auto script = item.GetObject();
			auto entry = CreateLiveMCM(std::move(script), static_cast<std::int32_t>(configIndex));
			if (!entry) {
				SKSE::log::debug("SkyUI registry skipped entry {}: {}", configIndex, entry.error().message);
				continue;
			}
			result.push_back(std::move(*entry));
		}
		return result;
	}

	bool ClassicRegistryProvider::IsBusy() const
	{
		auto        manager = ReadManager();
		const auto* active = manager ? manager->GetVariable(RE::BSFixedString("_activeConfig")) : nullptr;
		return active && active->IsObject() && active->GetObject();
	}

	bool ClassicRegistryProvider::IsAvailable() const
	{
		return RE::TESForm::LookupByEditorID<RE::TESQuest>("SKI_ConfigManagerInstance") != nullptr;
	}

	std::string_view ClassicRegistryProvider::Name() const
	{
		return "SkyUI";
	}

	RE::BSTSmartPointer<RE::BSScript::Object> ClassicRegistryProvider::ReadManager() const
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
