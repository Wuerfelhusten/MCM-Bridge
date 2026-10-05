#pragma once

#include "MCMBridge/Core/NativeMCMRegistry.h"
#include "RE/Skyrim.h"

#include <span>

namespace MCMBridge
{
	struct NativeObjectBinding
	{
		std::uint64_t                             instance{};
		RE::BSTSmartPointer<RE::BSScript::Object> object;
	};

	struct NativeRegistryMirror
	{
		std::uint64_t                            session{};
		std::uint64_t                            revision{};
		RE::BSTSmartPointer<RE::BSScript::Array> configs;
		RE::BSTSmartPointer<RE::BSScript::Array> names;
	};

	// Game task only. Builds owned temporary arrays; never writes manager fields or _configID.
	Result<NativeRegistryMirror> BuildNativeRegistryMirror(RE::BSScript::IVirtualMachine& a_vm,
		const NativeRegistryView& a_view, std::uint64_t a_session, std::span<const NativeObjectBinding> a_bindings);
}
