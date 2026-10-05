#include "MCMBridge/Papyrus/NativeHostPreflight.h"

namespace
{
	using RawType = RE::BSScript::TypeInfo::RawType;

	bool Probe(RE::BSScript::IVirtualMachine& a_vm, RawType a_type, std::string_view a_className, std::uint32_t a_size)
	{
		RE::BSTSmartPointer<RE::BSScript::Array> array;
		if (!a_vm.CreateArray(a_type, RE::BSFixedString(a_className), a_size, array) || !array || array->size() != a_size) {
			SKSE::log::error("Native host preflight: type={} requested={} actual={} allocation failed", static_cast<std::size_t>(a_type), a_size, array ? array->size() : 0);
			return false;
		}
		for (std::uint32_t index = 0; index < a_size; ++index) {
			if (a_type == RawType::kInt)
				(*array)[index].SetSInt(static_cast<std::int32_t>(index));
			else if (a_type == RawType::kString)
				(*array)[index].SetString(std::to_string(index));
		}
		RE::BSScript::Variable mirror;
		mirror.SetArray(array);
		const auto restored = mirror.GetArray();
		if (!restored || restored.get() != array.get() || restored->size() != a_size)
			return false;
		for (std::uint32_t index = 0; index < a_size; ++index) {
			if (a_type == RawType::kInt && (*restored)[index].GetSInt() != static_cast<std::int32_t>(index))
				return false;
			if (a_type == RawType::kString && std::string_view((*restored)[index].GetString()) != std::to_string(index))
				return false;
		}
		SKSE::log::info("Native host preflight: type={} class={} slots={} temporary array passed", static_cast<std::size_t>(a_type), a_className, a_size);
		return true;
	}
}

namespace MCMBridge
{
	void RunNativeHostPreflight()
	{
		SKSE::log::info("Native host preflight: game task started");
		spdlog::default_logger()->flush();
		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		if (!vm) {
			SKSE::log::error("Native host preflight: VM unavailable");
			spdlog::default_logger()->flush();
			return;
		}
		try {
			bool passed = true;
			for (const auto size : { 127U, 128U, 129U, 256U, 2000U }) {
				for (const auto type : { RawType::kInt, RawType::kString, RawType::kObject }) {
					// Persist the boundary before entering the VM, including abnormal process termination.
					SKSE::log::info("Native host preflight: begin type={} slots={}", static_cast<std::size_t>(type), size);
					spdlog::default_logger()->flush();
					const bool result = Probe(*vm, type, type == RawType::kObject ? "SKI_ConfigBase" : "", size);
					SKSE::log::info("Native host preflight: end type={} slots={} passed={}", static_cast<std::size_t>(type), size, result);
					spdlog::default_logger()->flush();
					if (!result)
						passed = false;
				}
			}
			SKSE::log::info("Native host preflight: temporary storage passed={}; object binding, Helper access and save roundtrip remain unverified; native host not activated", passed);
		} catch (const std::exception& error) {
			SKSE::log::error("Native host preflight failed: {}", error.what());
		}
		spdlog::default_logger()->flush();
	}
}
