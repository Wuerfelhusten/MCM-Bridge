#include "MCMBridge/Discovery/LiveMCMFactory.h"
#include "MCMBridge/Discovery/NativeRegistryProvider.h"

namespace MCMBridge
{
	Result<std::string> NativeRegistryProvider::ResolveIdentity(const RE::BSTSmartPointer<RE::BSScript::Object>& a_menu) const
	{
		if (!IsAvailable() || !a_menu || !a_menu->GetTypeInfo())
			return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "Native MCM registration is unavailable" });
		const auto binding = std::ranges::find_if(bindings, [&](const auto& a_value) { return a_value.object == a_menu; });
		const auto slot = binding != bindings.end() ? registry.FindSlot(registrySession, binding->instance) : std::nullopt;
		const auto entry = slot ? registry.Find(registrySession, *slot) : std::nullopt;
		if (!entry)
			return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "The requesting script is not registered" });
		auto*                                     vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		RE::BSTSmartPointer<RE::BSScript::Object> current;
		if (!vm || !vm->FindBoundObject(a_menu->GetHandle(), a_menu->GetTypeInfo()->GetName(), current) || current != a_menu)
			return std::unexpected(BridgeError{ BridgeErrorCode::kStaleSnapshot, "The requesting script is no longer bound" });
		const auto live = CreateLiveMCM(a_menu);
		if (!live || live->descriptor.stableID != entry->identity)
			return std::unexpected(BridgeError{ BridgeErrorCode::kStaleSnapshot, "The requesting MCM identity changed" });
		return entry->identity;
	}
}
