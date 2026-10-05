#include "MCMBridge/Papyrus/NativeFacade.h"
#include "MCMBridge/Papyrus/NativeFacadeBinding.h"

namespace
{
	std::atomic_bool facadeReady{};
	template <class Result, class Operation>
	Result GuardNative(Operation a_operation, Result a_failure)
	{
		try {
			return a_operation();
		} catch (const std::exception& error) {
			SKSE::log::error("Native MCM facade rejected call: {}", error.what());
			return a_failure;
		}
	}

	std::int32_t ProtocolVersion(RE::StaticFunctionTag*) { return facadeReady.load() ? 2 : 0; }

	bool BeginPage(RE::StaticFunctionTag*, std::int32_t a_token, RE::BSFixedString a_page, std::int32_t a_index)
	{
		return GuardNative([&] { return MCMBridge::NativeFacadeSession().BeginPage(a_token, a_page.c_str(), a_index); }, false);
	}

	bool SetHostCursor(RE::StaticFunctionTag*, std::int32_t a_token, std::int32_t a_position, std::int32_t a_fillMode)
	{
		return GuardNative([&] { return MCMBridge::NativeFacadeSession().SetCursor(a_token, a_position, a_fillMode); }, false);
	}

	std::int32_t AddOption(RE::StaticFunctionTag*, std::int32_t a_token, std::int32_t a_type, RE::BSFixedString a_label,
		RE::BSFixedString a_text, float a_value, std::int32_t a_flags, RE::BSFixedString a_state)
	{
		return GuardNative([&] { return MCMBridge::NativeFacadeSession().AddOption(a_token, a_type, a_label.c_str(), a_text.c_str(), a_value, a_flags, a_state.c_str()); }, std::int32_t{ -1 });
	}

	bool SetValue(RE::StaticFunctionTag*, std::int32_t a_token, std::int32_t a_optionID, RE::BSFixedString a_text, float a_value)
	{
		return GuardNative([&] { return MCMBridge::NativeFacadeSession().SetValue(a_token, a_optionID, a_text.c_str(), a_value); }, false);
	}

	bool SetFlags(RE::StaticFunctionTag*, std::int32_t a_token, std::int32_t a_optionID, std::int32_t a_flags)
	{
		return GuardNative([&] { return MCMBridge::NativeFacadeSession().SetFlags(a_token, a_optionID, a_flags); }, false);
	}

	bool PublishPage(RE::StaticFunctionTag*, std::int32_t a_token)
	{
		return GuardNative([&] { return MCMBridge::NativeFacadeSession().Publish(a_token); }, false);
	}

	bool SetSliderParameter(RE::StaticFunctionTag*, std::int32_t a_request, std::int32_t a_index, float a_value)
	{
		return GuardNative([&] { return MCMBridge::NativeFacadeSession().SetSliderParameter(a_request, a_index, a_value); }, false);
	}

	bool SetDialogIndex(RE::StaticFunctionTag*, std::int32_t a_request, std::int32_t a_type, std::int32_t a_index, std::int32_t a_value)
	{
		return GuardNative([&] { return MCMBridge::NativeFacadeSession().SetDialogIndex(a_request, a_type, a_index, a_value); }, false);
	}

	bool SetDialogOptions(RE::StaticFunctionTag*, std::int32_t a_request, std::vector<RE::BSFixedString> a_options)
	{
		return GuardNative([&] {
			std::vector<std::string> options;
			options.reserve(a_options.size());
			for (const auto& option : a_options)
				options.emplace_back(option.c_str());
			return MCMBridge::NativeFacadeSession().SetDialogOptions(a_request, std::move(options));
		},
			false);
	}

	bool SetDialogInput(RE::StaticFunctionTag*, std::int32_t a_request, RE::BSFixedString a_text)
	{
		return GuardNative([&] { return MCMBridge::NativeFacadeSession().SetDialogInput(a_request, a_text.c_str()); }, false);
	}
}

namespace MCMBridge
{
	NativeHostSession& NativeFacadeSession()
	{
		static NativeHostSession session;
		return session;
	}

	bool RegisterNativeFacade(RE::BSScript::IVirtualMachine* a_vm)
	{
		facadeReady.store(false);
		if (!a_vm)
			return false;
		// Protocol calls only touch synchronized host memory. No task is queued per control.
		NativeFacadeBinding binding(*a_vm);
		binding.RegisterFunction("GetProtocolVersion", ProtocolVersion);
		binding.RegisterFunction("BeginPage", BeginPage);
		binding.RegisterFunction("SetCursor", SetHostCursor);
		binding.RegisterFunction("AddOption", AddOption);
		binding.RegisterFunction("SetValue", SetValue);
		binding.RegisterFunction("SetFlags", SetFlags);
		binding.RegisterFunction("PublishPage", PublishPage);
		binding.RegisterFunction("SetSliderParameter", SetSliderParameter);
		binding.RegisterFunction("SetDialogIndex", SetDialogIndex);
		binding.RegisterFunction("SetDialogOptions", SetDialogOptions);
		binding.RegisterFunction("SetDialogInput", SetDialogInput);
		const bool runtime = RegisterNativeFacadeRuntime(a_vm);
		const bool navigation = RegisterNativePageAdmission(*a_vm);
		const bool calls = RegisterNativeCallAdmission(*a_vm);
		const bool ready = binding.IsValid() && runtime && navigation && calls;
		facadeReady.store(ready);
		return ready;
	}

	bool IsNativeFacadeReady() { return facadeReady.load(); }
}
