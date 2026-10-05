#include "MCMBridge/UI/KeymapCatalog.h"

#include <Windows.h>

#include <array>
#include <format>

namespace
{
	std::string ToUtf8(const wchar_t* a_text, int a_length)
	{
		if (!a_text || a_length <= 0) {
			return {};
		}
		const auto required = WideCharToMultiByte(CP_UTF8, 0, a_text, a_length, nullptr, 0, nullptr, nullptr);
		if (required <= 0) {
			return {};
		}
		std::string result(static_cast<std::size_t>(required), '\0');
		WideCharToMultiByte(CP_UTF8, 0, a_text, a_length, result.data(), required, nullptr, nullptr);
		return result;
	}

	std::string KeyboardName(std::int32_t a_keyCode)
	{
		auto scanCode = static_cast<std::uint32_t>(a_keyCode) & 0xFF;
		switch (scanCode) {
		case 0x9C:
			scanCode = 0x11C;
			break;
		case 0x9D:
			scanCode = 0x11D;
			break;
		case 0xB5:
			scanCode = 0x135;
			break;
		case 0xB8:
			scanCode = 0x138;
			break;
		case 0xC7:
		case 0xC8:
		case 0xC9:
		case 0xCB:
		case 0xCD:
		case 0xCF:
		case 0xD0:
		case 0xD1:
		case 0xD2:
		case 0xD3:
			scanCode += 0x80;
			break;
		default:
			break;
		}

		auto parameter = static_cast<LONG>(scanCode << 16);
		if (scanCode > 0xFF || scanCode == 0x45) {
			parameter |= 1 << 24;
		}
		std::array<wchar_t, 128> buffer{};
		const auto               length = GetKeyNameTextW(parameter, buffer.data(), static_cast<int>(buffer.size()));
		return ToUtf8(buffer.data(), length);
	}

	constexpr std::array mouseNames{
		"Left Mouse Button", "Right Mouse Button", "Middle Mouse Button", "Mouse Button 3",
		"Mouse Button 4", "Mouse Button 5", "Mouse Button 6", "Mouse Button 7",
		"Mouse Wheel Up", "Mouse Wheel Down"
	};

	constexpr std::array gamepadNames{
		"Gamepad DPad Up", "Gamepad DPad Down", "Gamepad DPad Left", "Gamepad DPad Right",
		"Gamepad Start", "Gamepad Back", "Gamepad Left Thumb", "Gamepad Right Thumb",
		"Gamepad Left Shoulder", "Gamepad Right Shoulder", "Gamepad A", "Gamepad B",
		"Gamepad X", "Gamepad Y", "Gamepad LT", "Gamepad RT"
	};
}

namespace MCMBridge::KeymapCatalog
{
	std::string Name(std::int32_t a_keyCode)
	{
		if (a_keyCode < 0) {
			return "Unmapped";
		}
		if (a_keyCode < 256) {
			auto name = KeyboardName(a_keyCode);
			return name.empty() ? std::format("Key {}", a_keyCode) : name;
		}
		if (a_keyCode < 266) {
			return mouseNames[static_cast<std::size_t>(a_keyCode - 256)];
		}
		if (a_keyCode < 282) {
			return gamepadNames[static_cast<std::size_t>(a_keyCode - 266)];
		}
		return std::format("Key {}", a_keyCode);
	}
}
