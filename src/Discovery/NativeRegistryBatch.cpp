#include "MCMBridge/Discovery/LiveMCMFactory.h"
#include "MCMBridge/Discovery/NativeRegistryProvider.h"
#include "MCMBridge/Papyrus/NativeFacade.h"

#include <limits>

namespace MCMBridge
{
	Result<std::vector<std::int32_t>> NativeRegistryProvider::RegisterBatch(std::span<const NativeMenuRegistration> a_menus)
	{
		const auto started = std::chrono::steady_clock::now();
		auto*      vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		if (!IsAvailable() || !vm)
			return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "Native registry batch is unavailable" });
		if (a_menus.empty())
			return std::vector<std::int32_t>{};
		if (a_menus.size() == 1) {
			auto slot = Register(a_menus.front().object, a_menus.front().name);
			if (!slot)
				return std::unexpected(slot.error());
			return std::vector<std::int32_t>{ *slot };
		}
		auto                                                           view = registry.Read();
		auto                                                           objects = bindings;
		auto                                                           next = nextInstance;
		std::unordered_map<const RE::BSScript::Object*, std::uint64_t> byObject;
		std::unordered_map<std::uint64_t, std::size_t>                 byInstance;
		for (const auto& binding : objects)
			byObject.emplace(binding.object.get(), binding.instance);
		for (std::size_t index = 0; index < view.slots.size(); ++index)
			if (view.slots[index])
				byInstance.emplace(view.slots[index]->instance, index);
		std::vector<NativeMCMEntry> entries;
		entries.reserve(a_menus.size());
		for (const auto& menu : a_menus) {
			if (!HasNativeFacadeContract(menu.object))
				return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "Native batch requires compatible MCM facades" });
			auto live = CreateLiveMCM(menu.object, -1);
			if (!live)
				return std::unexpected(live.error());
			auto found = byObject.find(menu.object.get());
			if (found == byObject.end()) {
				if (next == std::numeric_limits<std::uint64_t>::max())
					return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "Native MCM instance tokens exhausted" });
				found = byObject.emplace(menu.object.get(), ++next).first;
				objects.push_back({ found->second, menu.object });
			}
			NativeMCMEntry entry{ found->second, live->descriptor.stableID, menu.name };
			if (const auto existing = byInstance.find(entry.instance); existing != byInstance.end()) {
				auto& previous = *view.slots[existing->second];
				if (previous.identity != entry.identity)
					return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Native batch instance identity changed" });
				previous = entry;
			} else {
				byInstance.emplace(entry.instance, view.slots.size());
				view.slots.push_back(entry);
				++view.count;
			}
			entries.push_back(std::move(entry));
		}
		// Build and validate the complete mirrors before making any registration visible.
		auto mirror = BuildNativeRegistryMirror(*vm, view, registrySession, objects);
		if (!mirror)
			return std::unexpected(mirror.error());
		auto slots = registry.RegisterBatch(registrySession, std::move(entries));
		if (!slots)
			return std::unexpected(slots.error());
		mirror->revision = registry.Revision();
		if (!Install(view, std::move(*mirror))) {
			Reset();
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "Native batch mirror installation failed" });
		}
		bindings = std::move(objects);
		nextInstance = next;
		if (a_menus.size() > 1)
			SKSE::log::info("Native registry group installed: requests={} total={} mirror_builds=1 elapsed_ms={:.3f}",
				a_menus.size(), view.count, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count());
		return slots;
	}
}
