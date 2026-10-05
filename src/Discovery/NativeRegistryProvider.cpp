#include "MCMBridge/Discovery/NativeRegistryProvider.h"

#include "MCMBridge/Discovery/LiveMCMFactory.h"
#include "MCMBridge/Papyrus/MCMScript.h"
#include "MCMBridge/Papyrus/NativeFacade.h"
#include "MCMBridge/Papyrus/NativeHostScript.h"

#include <algorithm>
#include <limits>

namespace
{
	using namespace MCMBridge;

	bool HasNativeFacade(const RE::BSTSmartPointer<RE::BSScript::Object>& a_script)
	{
		return HasNativeFacadeContract(a_script);
	}

	class NativeAdapter final : public IMCMHostAdapter
	{
	public:
		NativeAdapter(std::uint64_t a_session, std::string a_id, RE::BSTSmartPointer<RE::BSScript::Object> a_script) :
			session(a_session), id(std::move(a_id)), script(std::move(a_script)) {}
		std::shared_ptr<IClassicScript> CreateSession() const override
		{
			auto result = CreateNativeHostScript(session, id, script);
			if (!result) {
				SKSE::log::error("Native host session rejected: {}", result.error().message);
				return {};
			}
			return std::move(*result);
		}
		bool IsValid() const override
		{
			auto*                                     vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			RE::BSTSmartPointer<RE::BSScript::Object> live;
			return vm && HasNativeFacade(script) && script->GetTypeInfo() &&
			       vm->FindBoundObject(script->GetHandle(), script->GetTypeInfo()->GetName(), live) && live == script;
		}
		std::optional<std::vector<std::string>> ReadRegisteredPages() const override
		{
			return IsValid() ? MCMScript(script).ReadNavigationPages() : std::nullopt;
		}

	private:
		std::uint64_t                             session;
		std::string                               id;
		RE::BSTSmartPointer<RE::BSScript::Object> script;
	};
}

namespace MCMBridge
{
	bool NativeRegistryProvider::SetActive(const RE::BSTSmartPointer<RE::BSScript::Object>& a_menu)
	{
		if (!IsAvailable() || !a_menu || !ResolveIdentity(a_menu))
			return false;
		auto* active = manager->GetVariable("_activeConfig");
		if (!active || (!active->IsObject() && !active->IsNoneObject()) ||
			(active->IsObject() && active->GetObject() && active->GetObject() != a_menu))
			return false;
		active->SetObject(a_menu);
		return true;
	}

	void NativeRegistryProvider::ClearActive(const RE::BSTSmartPointer<RE::BSScript::Object>& a_menu)
	{
		auto* active = manager ? manager->GetVariable("_activeConfig") : nullptr;
		if (active && active->IsObject() && active->GetObject() == a_menu)
			active->SetNone();
	}

	void NativeRegistryProvider::Reset()
	{
		session = 0;
		manager.reset();
		installedMirror = {};
		installedCount = 0;
		bindings.clear();
		registrySession = registry.BeginSession();
		nextInstance = 0;
	}

	bool NativeRegistryProvider::Install(const NativeRegistryView& a_view, NativeRegistryMirror a_mirror)
	{
		auto* configs = manager->GetVariable("_modConfigs");
		auto* names = manager->GetVariable("_modNames");
		auto* count = manager->GetVariable("_configCount");
		if (!configs || !names || !count || !count->IsInt())
			return false;
		configs->SetArray(a_mirror.configs);
		names->SetArray(a_mirror.names);
		count->SetSInt(static_cast<std::int32_t>(a_view.count));
		installedMirror = std::move(a_mirror);
		installedCount = static_cast<std::int32_t>(a_view.count);
		return true;
	}

	bool NativeRegistryProvider::MirrorMatches(std::int32_t a_slot,
		const RE::BSTSmartPointer<RE::BSScript::Object>& a_menu, std::string_view a_name) const
	{
		const auto* configs = manager->GetVariable("_modConfigs");
		const auto* names = manager->GetVariable("_modNames");
		const auto* count = manager->GetVariable("_configCount");
		if (!configs || !configs->IsArray() || !names || !names->IsArray() ||
			!count || !count->IsInt() || count->GetSInt() != installedCount ||
			configs->GetArray() != installedMirror.configs || names->GetArray() != installedMirror.names ||
			!installedMirror.configs || !installedMirror.names || a_slot < 0)
			return false;
		const auto slot = static_cast<std::uint32_t>(a_slot);
		if (slot >= installedMirror.configs->size() || slot >= installedMirror.names->size())
			return false;
		const auto& object = (*installedMirror.configs)[slot];
		const auto& name = (*installedMirror.names)[slot];
		return object.IsObject() && object.GetObject() == a_menu && name.IsString() &&
		       std::string_view(name.GetString()) == a_name;
	}

	Result<bool> NativeRegistryProvider::Clear()
	{
		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		if (!IsAvailable() || !vm)
			return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "Native registry is inactive" });
		NativeRegistryView empty{ .session = registrySession, .revision = 1 };
		auto               mirror = BuildNativeRegistryMirror(*vm, empty, registrySession, {});
		if (!mirror)
			return std::unexpected(mirror.error());
		if (!Install(empty, std::move(*mirror)))
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Native registry reset could not install empty mirrors" });
		registrySession = registry.BeginSession();
		bindings.clear();
		return true;
	}

	Result<bool> NativeRegistryProvider::Bootstrap(RE::BSTSmartPointer<RE::BSScript::Object> a_manager, std::uint64_t a_session)
	{
		if (!a_session || !a_manager || IsAvailable())
			return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "Native registry bootstrap is not admissible" });
		auto*       vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		const auto* version = a_manager->GetProperty("MCMBridgeManagerVersion");
		const auto* configs = a_manager->GetVariable("_modConfigs");
		const auto* names = a_manager->GetVariable("_modNames");
		const auto* count = a_manager->GetVariable("_configCount");
		std::string reason;
		if (!HasNativeFacadeContract(a_manager, true, &reason))
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Native manager admission failed: " + reason });
		if (!vm || !version || !configs || !configs->IsArray() || !names || !names->IsArray() || !count || !count->IsInt())
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Native manager compatibility fields are missing" });
		Reset();
		NativeRegistryView               view{ .session = registrySession, .revision = 1 };
		std::vector<NativeObjectBinding> objects;
		const auto                       saved = configs->GetArray();
		if (saved) {
			view.slots.resize(saved->size());
			for (std::uint32_t index = 0; index < saved->size(); ++index) {
				const auto& value = (*saved)[index];
				if (!value.IsObject())
					continue;
				auto object = value.GetObject();
				if (!object)
					continue;
				if (!HasNativeFacade(object))
					return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable,
						"Registered MCM has a missing or incompatible native facade; registry migration was not applied" });
				auto live = CreateLiveMCM(object);
				if (!live) {
					SKSE::log::warn("Native bootstrap skipped slot {}: {}", index, live.error().message);
					continue;
				}
				const auto instance = ++nextInstance;
				view.slots[index] = NativeMCMEntry{ instance, live->descriptor.stableID, live->descriptor.displayName };
				objects.push_back({ instance, object });
				++view.count;
			}
		}
		auto mirror = BuildNativeRegistryMirror(*vm, view, registrySession, objects);
		if (!mirror)
			return std::unexpected(mirror.error());
		auto imported = registry.Import(registrySession, view.slots);
		if (!imported)
			return std::unexpected(imported.error());
		manager = std::move(a_manager);
		if (!Install(view, std::move(*mirror))) {
			Reset();
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Native manager fields changed during bootstrap" });
		}
		bindings = std::move(objects);
		session = a_session;
		return true;
	}

	Result<std::int32_t> NativeRegistryProvider::Register(RE::BSTSmartPointer<RE::BSScript::Object> a_menu, std::string a_name)
	{
		if (!IsAvailable())
			return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "Native registry is inactive" });
		if (!HasNativeFacade(a_menu))
			return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "MCM registration requires a compatible native facade" });
		auto live = CreateLiveMCM(a_menu);
		if (!live)
			return std::unexpected(live.error());
		const auto known = std::ranges::find_if(bindings, [&](const auto& a_binding) { return a_binding.object == a_menu; });
		if (known != bindings.end()) {
			const auto slot = registry.FindSlot(registrySession, known->instance);
			const auto entry = slot ? registry.Find(registrySession, *slot) : std::nullopt;
			if (entry && entry->identity == live->descriptor.stableID && entry->name == a_name &&
				MirrorMatches(*slot, a_menu, a_name)) {
				auto*                                     vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
				RE::BSTSmartPointer<RE::BSScript::Object> bound;
				if (vm && vm->FindBoundObject(a_menu->GetHandle(), a_menu->GetTypeInfo()->GetName(), bound) && bound == a_menu)
					return *slot;
			}
		}
		if (known == bindings.end() && nextInstance == std::numeric_limits<std::uint64_t>::max())
			return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "Native MCM instance tokens exhausted" });
		auto       objects = bindings;
		const auto instance = known == bindings.end() ? ++nextInstance : known->instance;
		if (known == bindings.end())
			objects.push_back({ instance, a_menu });
		NativeMCMEntry entry{ instance, live->descriptor.stableID, std::move(a_name) };
		auto           view = registry.Read();
		const auto     existing = std::ranges::find_if(view.slots, [&](const auto& a_slot) { return a_slot && a_slot->instance == instance; });
		if (existing != view.slots.end())
			*existing = entry;
		else {
			view.slots.push_back(entry);
			++view.count;
		}
		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		if (!vm)
			return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "Papyrus is unavailable" });
		auto mirror = BuildNativeRegistryMirror(*vm, view, registrySession, objects);
		if (!mirror)
			return std::unexpected(mirror.error());
		auto slot = registry.Register(registrySession, std::move(entry));
		if (!slot)
			return std::unexpected(slot.error());
		if (!Install(view, std::move(*mirror))) {
			Reset();
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Native registry mirror installation failed" });
		}
		bindings = std::move(objects);
		return *slot;
	}

	Result<std::int32_t> NativeRegistryProvider::Unregister(const RE::BSTSmartPointer<RE::BSScript::Object>& a_menu)
	{
		if (!IsAvailable())
			return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "Native registry is inactive" });
		auto       objects = bindings;
		const auto binding = std::ranges::find_if(objects, [&](const auto& a_binding) { return a_binding.object == a_menu; });
		if (binding == objects.end())
			return -1;
		const auto instance = binding->instance;
		objects.erase(binding);
		auto       view = registry.Read();
		const auto entry = std::ranges::find_if(view.slots, [&](const auto& a_slot) { return a_slot && a_slot->instance == instance; });
		if (entry == view.slots.end())
			return -1;
		const auto slot = static_cast<std::int32_t>(entry - view.slots.begin());
		entry->reset();
		--view.count;
		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		if (!vm)
			return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "Papyrus is unavailable" });
		auto mirror = BuildNativeRegistryMirror(*vm, view, registrySession, objects);
		if (!mirror)
			return std::unexpected(mirror.error());
		auto removed = registry.Unregister(registrySession, instance);
		if (!removed)
			return std::unexpected(removed.error());
		if (!Install(view, std::move(*mirror))) {
			Reset();
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Native registry mirror removal failed" });
		}
		bindings = std::move(objects);
		return slot;
	}

	Result<std::vector<LiveMCM>> NativeRegistryProvider::ReadLive()
	{
		if (!IsAvailable())
			return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "Native registry is inactive" });
		std::vector<LiveMCM> result;
		const auto           view = registry.Read();
		result.reserve(view.count);
		std::unordered_map<std::uint64_t, const NativeObjectBinding*> byInstance;
		byInstance.reserve(bindings.size());
		for (const auto& binding : bindings)
			byInstance.emplace(binding.instance, &binding);
		for (std::size_t slot = 0; slot < view.slots.size(); ++slot) {
			if (!view.slots[slot])
				continue;
			const auto& entry = *view.slots[slot];
			const auto  binding = byInstance.find(entry.instance);
			if (binding == byInstance.end())
				continue;
			auto live = CreateLiveMCM(binding->second->object);
			if (!live)
				continue;
			live->adapter = std::make_shared<NativeAdapter>(session, entry.identity, binding->second->object);
			result.push_back(std::move(*live));
		}
		return result;
	}

	Result<std::size_t> NativeRegistryProvider::Count() const
	{
		if (!IsAvailable())
			return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "Native registry is inactive" });
		return registry.Count(registrySession);
	}

	Result<std::vector<MCMDescriptor>> NativeRegistryProvider::Read()
	{
		auto live = ReadLive();
		if (!live)
			return std::unexpected(live.error());
		std::vector<MCMDescriptor> result;
		for (auto& entry : *live) result.push_back(std::move(entry.descriptor));
		return result;
	}
}
