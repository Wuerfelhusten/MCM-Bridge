#include "MCMBridge/Write/KeymapConflictResolver.h"

namespace
{
	std::optional<std::pair<RE::INPUT_DEVICE, std::uint32_t>> DecodeKey(std::int32_t a_keyCode)
	{
		if (a_keyCode >= 0 && a_keyCode < 256) {
			return std::pair{ RE::INPUT_DEVICE::kKeyboard, static_cast<std::uint32_t>(a_keyCode) };
		}
		if (a_keyCode >= 256 && a_keyCode <= 265) {
			return std::pair{ RE::INPUT_DEVICE::kMouse, static_cast<std::uint32_t>(a_keyCode - 256) };
		}
		using Key = RE::BSWin32GamepadDevice::Key;
		switch (a_keyCode) {
		case 266:
			return std::pair{ RE::INPUT_DEVICE::kGamepad, static_cast<std::uint32_t>(Key::kUp) };
		case 267:
			return std::pair{ RE::INPUT_DEVICE::kGamepad, static_cast<std::uint32_t>(Key::kDown) };
		case 268:
			return std::pair{ RE::INPUT_DEVICE::kGamepad, static_cast<std::uint32_t>(Key::kLeft) };
		case 269:
			return std::pair{ RE::INPUT_DEVICE::kGamepad, static_cast<std::uint32_t>(Key::kRight) };
		case 270:
			return std::pair{ RE::INPUT_DEVICE::kGamepad, static_cast<std::uint32_t>(Key::kStart) };
		case 271:
			return std::pair{ RE::INPUT_DEVICE::kGamepad, static_cast<std::uint32_t>(Key::kBack) };
		case 272:
			return std::pair{ RE::INPUT_DEVICE::kGamepad, static_cast<std::uint32_t>(Key::kLeftThumb) };
		case 273:
			return std::pair{ RE::INPUT_DEVICE::kGamepad, static_cast<std::uint32_t>(Key::kRightThumb) };
		case 274:
			return std::pair{ RE::INPUT_DEVICE::kGamepad, static_cast<std::uint32_t>(Key::kLeftShoulder) };
		case 275:
			return std::pair{ RE::INPUT_DEVICE::kGamepad, static_cast<std::uint32_t>(Key::kRightShoulder) };
		case 276:
			return std::pair{ RE::INPUT_DEVICE::kGamepad, static_cast<std::uint32_t>(Key::kA) };
		case 277:
			return std::pair{ RE::INPUT_DEVICE::kGamepad, static_cast<std::uint32_t>(Key::kB) };
		case 278:
			return std::pair{ RE::INPUT_DEVICE::kGamepad, static_cast<std::uint32_t>(Key::kX) };
		case 279:
			return std::pair{ RE::INPUT_DEVICE::kGamepad, static_cast<std::uint32_t>(Key::kY) };
		case 280:
			return std::pair{ RE::INPUT_DEVICE::kGamepad, static_cast<std::uint32_t>(Key::kLeftTrigger) };
		case 281:
			return std::pair{ RE::INPUT_DEVICE::kGamepad, static_cast<std::uint32_t>(Key::kRightTrigger) };
		default:
			return std::nullopt;
		}
	}
}

namespace MCMBridge
{
	std::pair<std::string, std::string> ResolveKeymapConflict(
		const MCMSnapshot& a_snapshot,
		const MCMControl&  a_control,
		std::int32_t       a_keyCode)
	{
		if (a_control.ignoreConflicts || a_keyCode < 0) {
			return {};
		}
		if (const auto decoded = DecodeKey(a_keyCode)) {
			if (const auto* map = RE::ControlMap::GetSingleton()) {
				const auto eventName = map->GetUserEventName(decoded->second, decoded->first);
				if (!eventName.empty()) {
					return { std::string(eventName), {} };
				}
			}
		}
		for (const auto& mod : a_snapshot.mods) {
			for (const auto& page : mod.pages) {
				for (const auto& candidate : page.controls) {
					if (candidate.type != MCMControlType::kKeymap ||
						candidate.identity.stableID == a_control.identity.stableID) {
						continue;
					}
					const auto* mapped = std::get_if<std::int32_t>(&candidate.value);
					if (mapped && *mapped == a_keyCode) {
						return { candidate.label, mod.displayName };
					}
				}
			}
		}
		return {};
	}
}
