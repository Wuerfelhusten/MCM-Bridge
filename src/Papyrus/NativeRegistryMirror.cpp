#include "MCMBridge/Papyrus/NativeRegistryMirror.h"
#include "MCMBridge/Core/NativePreflightPolicy.h"

#include <limits>
#include <unordered_set>

namespace MCMBridge
{
	Result<NativeRegistryMirror> BuildNativeRegistryMirror(RE::BSScript::IVirtualMachine& a_vm,
		const NativeRegistryView& a_view, std::uint64_t a_session, std::span<const NativeObjectBinding> a_bindings)
	{
		if (!a_session || a_session != a_view.session)
			return std::unexpected(BridgeError{ BridgeErrorCode::kStaleSnapshot, "Native mirror session changed" });
		if (a_view.slots.size() > static_cast<std::size_t>(std::numeric_limits<std::int32_t>::max()))
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Native mirror slot count exceeds the compatibility range" });
		std::unordered_map<std::uint64_t, RE::BSTSmartPointer<RE::BSScript::Object>> objects;
		std::unordered_set<const RE::BSScript::Object*>                              uniqueObjects;
		objects.reserve(a_bindings.size());
		uniqueObjects.reserve(a_bindings.size());
		for (const auto& binding : a_bindings) {
			if (!binding.instance || !binding.object || !binding.object->GetTypeInfo() ||
				!objects.emplace(binding.instance, binding.object).second || !uniqueObjects.insert(binding.object.get()).second)
				return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Native mirror object binding is missing or duplicated" });
			bool compatible = false;
			for (auto* type = binding.object->GetTypeInfo(); type; type = type->GetParent()) {
				if (EqualPapyrusTypeName(type->GetName(), "SKI_ConfigBase")) {
					compatible = true;
					break;
				}
			}
			const auto context = [&] {
				const auto entry = std::ranges::find_if(a_view.slots, [&](const auto& a_slot) { return a_slot && a_slot->instance == binding.instance; });
				const auto identity = entry != a_view.slots.end() ? (*entry)->identity : "<unmapped>";
				return std::format("identity={} script={}", identity, binding.object->GetTypeInfo()->GetName());
			};
			if (!compatible) {
				std::string ancestry;
				for (auto* type = binding.object->GetTypeInfo(); type; type = type->GetParent()) {
					if (!ancestry.empty())
						ancestry += " -> ";
					ancestry += type->GetName();
				}
				return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, std::format("Native mirror incompatible base: {}; ancestry={}", context(), ancestry) });
			}
			RE::BSTSmartPointer<RE::BSScript::Object> live;
			if (!a_vm.FindBoundObject(binding.object->GetHandle(), binding.object->GetTypeInfo()->GetName(), live) || !live)
				return std::unexpected(BridgeError{ BridgeErrorCode::kStaleSnapshot, std::format("Native mirror binding missing: {}", context()) });
			if (live != binding.object)
				return std::unexpected(BridgeError{ BridgeErrorCode::kStaleSnapshot, std::format("Native mirror binding points to a different object: {}", context()) });
		}
		std::unordered_set<std::uint64_t> used;
		std::unordered_set<std::string>   identities;
		used.reserve(objects.size());
		identities.reserve(objects.size());
		for (const auto& slot : a_view.slots) {
			if (!slot)
				continue;
			if (!objects.contains(slot->instance) || !used.insert(slot->instance).second || slot->identity.empty() || !identities.insert(slot->identity).second)
				return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Native mirror registry and bindings disagree" });
		}
		if (used.size() != a_view.count || used.size() != objects.size())
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Native mirror object count mismatch" });
		NativeRegistryMirror mirror{ .session = a_session, .revision = a_view.revision };
		const auto           size = static_cast<std::uint32_t>(a_view.slots.size());
		using RawType = RE::BSScript::TypeInfo::RawType;
		if (!a_vm.CreateArray(RawType::kObject, "SKI_ConfigBase", size, mirror.configs) ||
			!a_vm.CreateArray(RawType::kString, "", size, mirror.names) || !mirror.configs || !mirror.names ||
			mirror.configs->size() != size || mirror.names->size() != size)
			return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "Native mirror array allocation failed" });
		for (std::uint32_t index = 0; index < size; ++index) {
			const auto& slot = a_view.slots[index];
			if (!slot)
				continue;
			auto& value = (*mirror.configs)[index];
			value.SetObject(objects.at(slot->instance), mirror.configs->type_info().GetRawType());
			(*mirror.names)[index].SetString(slot->name);
		}
		return mirror;
	}
}
