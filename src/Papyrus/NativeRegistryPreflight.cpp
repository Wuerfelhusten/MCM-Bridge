#include "MCMBridge/Papyrus/NativeHostPreflight.h"

#include "MCMBridge/Core/NativePreflightPolicy.h"
#include "MCMBridge/Discovery/LiveMCM.h"
#include "MCMBridge/Discovery/LiveMCMFactory.h"
#include "MCMBridge/Papyrus/NativeRegistryMirror.h"

namespace MCMBridge
{
	void RunNativeRegistryPreflight(std::span<const LiveMCM> a_entries, std::uint64_t a_session)
	{
		// The controller calls this on the game thread. No objects survive this diagnostic.
		static NativePreflightGate gate;
		std::vector<std::string>   identities;
		identities.reserve(a_entries.size());
		for (const auto& entry : a_entries) identities.push_back(entry.descriptor.stableID);
		if (!gate.ShouldRun(a_session, std::move(identities)))
			return;
		SKSE::log::info("Native registry preflight: begin session={} discovered={}; live registry will not be modified", a_session, a_entries.size());
		spdlog::default_logger()->flush();
		try {
			auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			auto* data = RE::TESDataHandler::GetSingleton();
			auto* policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
			if (!vm || !data || !policy)
				throw std::runtime_error("VM or data handler unavailable");
			NativeMCMRegistry                        registry;
			const auto                               session = registry.BeginSession();
			std::vector<NativeObjectBinding>         bindings;
			std::vector<std::optional<std::int32_t>> originalIDs;
			std::size_t                              helperCount = 0;
			for (const auto& entry : a_entries) {
				const auto&                               descriptor = entry.descriptor;
				auto*                                     quest = data->LookupForm<RE::TESQuest>(descriptor.questFormID, descriptor.ownerPlugin);
				RE::BSTSmartPointer<RE::BSScript::Object> object;
				if (!quest || !vm->FindBoundObject(policy->GetHandleForObject(quest->GetFormType(), quest), descriptor.scriptName.c_str(), object) || !object)
					throw std::runtime_error(std::format("Cannot resolve {}", descriptor.stableID));
				const auto verified = CreateLiveMCM(object);
				if (!verified || verified->descriptor.stableID != descriptor.stableID)
					throw std::runtime_error("Live MCM identity changed");
				const auto token = static_cast<std::uint64_t>(bindings.size()) + 1;
				const auto slot = registry.Register(session, { token, descriptor.stableID, verified->descriptor.displayName });
				if (!slot)
					throw std::runtime_error(slot.error().message);
				bindings.push_back({ token, object });
				const auto* configID = object->GetVariable("_configID");
				originalIDs.push_back(configID && configID->IsInt() ? std::optional(configID->GetSInt()) : std::nullopt);
				if (descriptor.backend == MCMBackendKind::kMCMHelper)
					++helperCount;
			}
			const auto view = registry.Read();
			auto       mirror = BuildNativeRegistryMirror(*vm, view, session, bindings);
			if (!mirror)
				throw std::runtime_error(mirror.error().message);
			RE::BSScript::Variable configs, names;
			if (BuildNativeRegistryMirror(*vm, view, session + 1, bindings))
				throw std::runtime_error("Stale mirror session was accepted");
			auto duplicateBindings = bindings;
			duplicateBindings.push_back(bindings.front());
			if (BuildNativeRegistryMirror(*vm, view, session, duplicateBindings))
				throw std::runtime_error("Duplicate object binding was accepted");
			configs.SetArray(mirror->configs);
			names.SetArray(mirror->names);
			for (std::uint32_t index = 0; index < mirror->configs->size(); ++index) {
				if ((*configs.GetArray())[index].GetObject() != bindings[index].object ||
					std::string_view((*names.GetArray())[index].GetString()) != view.slots[index]->name)
					throw std::runtime_error("Object or name roundtrip mismatch");
			}
			RE::BSTSmartPointer<RE::BSScript::Array> stress;
			if (!vm->CreateArray(RE::BSScript::TypeInfo::RawType::kObject, "SKI_ConfigBase", 2000, stress) || !stress || stress->size() != 2000)
				throw std::runtime_error("Object stress array allocation failed");
			for (std::uint32_t index = 0; index < 2000; ++index)
				(*stress)[index].SetObject(bindings[index % bindings.size()].object, stress->type_info().GetRawType());
			RE::BSScript::Variable stressMirror;
			stressMirror.SetArray(stress);
			stress.reset();
			for (std::uint32_t index = 0; index < 2000; ++index)
				if ((*stressMirror.GetArray())[index].GetObject() != bindings[index % bindings.size()].object)
					throw std::runtime_error("Object stress roundtrip mismatch");
			for (std::size_t index = 0; index < bindings.size(); ++index) {
				const auto* configID = bindings[index].object->GetVariable("_configID");
				const auto  current = configID && configID->IsInt() ? std::optional(configID->GetSInt()) : std::nullopt;
				if (current != originalIDs[index])
					throw std::runtime_error("Live config ID changed during diagnostic");
			}
			const auto removed = registry.Unregister(session, bindings.front().instance);
			if (!removed || !*removed)
				throw std::runtime_error("Local registry removal failed");
			const auto remaining = std::span<const NativeObjectBinding>(bindings).subspan(1);
			const auto sparse = BuildNativeRegistryMirror(*vm, registry.Read(), session, remaining);
			if (!sparse || sparse->configs->size() != mirror->configs->size() || (*sparse->configs)[0].GetObject())
				throw std::runtime_error("Removed mirror slot was not preserved as a hole");
			for (std::uint32_t index = 1; index < sparse->configs->size(); ++index)
				if ((*sparse->configs)[index].GetObject() != bindings[index].object)
					throw std::runtime_error("Removal shifted a compatibility slot");
			SKSE::log::info("Native registry preflight: passed=true unique_objects={} helper_objects={} repeated_reference_slots=2000; manager untouched; Helper execution and save/load unverified", bindings.size(), helperCount);
		} catch (const std::exception& error) {
			SKSE::log::error("Native registry preflight: passed=false reason={}", error.what());
		}
		spdlog::default_logger()->flush();
	}
}
