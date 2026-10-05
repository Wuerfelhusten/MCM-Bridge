#include "MCMBridge/Papyrus/NativeFacade.h"
#include "MCMBridge/Papyrus/NativeFacadeBinding.h"

#include "MCMBridge/UI/MessageDialog.h"

namespace
{
	template <class Operation, class Result>
	Result Guard(Operation a_operation, Result a_failure)
	{
		try {
			return a_operation();
		} catch (const std::exception& error) {
			SKSE::log::error("Native MCM lifecycle rejected call: {}", error.what());
			return a_failure;
		}
	}

	bool IsActive(RE::StaticFunctionTag*, std::int32_t a_token)
	{
		return Guard([&] { return MCMBridge::NativeFacadeSession().IsActive(a_token); }, false);
	}

	void LogError(RE::StaticFunctionTag*, RE::BSFixedString a_message)
	{
		Guard([&] { SKSE::log::error("Native MCM facade: {}", a_message.c_str()); return true; }, false);
	}

	std::int32_t BeginDialog(RE::StaticFunctionTag*, std::int32_t a_token, std::int32_t a_option)
	{
		return Guard([&] { return MCMBridge::NativeFacadeSession().BeginDialog(a_token, a_option); }, std::int32_t{});
	}

	bool FinishDialog(RE::StaticFunctionTag*, std::int32_t a_token, std::int32_t a_request)
	{
		return Guard([&] { return MCMBridge::NativeFacadeSession().FinishDialog(a_token, a_request).has_value(); }, false);
	}

	bool SetPresentation(RE::StaticFunctionTag*, std::int32_t a_token, std::int32_t a_kind, RE::BSFixedString a_text)
	{
		return Guard([&] { return MCMBridge::NativeFacadeSession().SetPresentation(a_token, a_kind, a_text.c_str()); }, false);
	}

	std::vector<std::string> Strings(const std::vector<RE::BSFixedString>& a_strings)
	{
		std::vector<std::string> result;
		result.reserve(a_strings.size());
		for (const auto& value : a_strings)
			result.emplace_back(value.c_str());
		return result;
	}

	bool ImportBuffers(RE::StaticFunctionTag*, std::int32_t a_token, std::vector<std::int32_t> a_flags,
		std::vector<RE::BSFixedString> a_labels, std::vector<RE::BSFixedString> a_strings,
		std::vector<float> a_numbers, std::vector<RE::BSFixedString> a_states)
	{
		return Guard([&] {
			return MCMBridge::NativeFacadeSession().ImportBuffers(a_token,
				{ std::move(a_flags), Strings(a_labels), Strings(a_strings), std::move(a_numbers), Strings(a_states) });
		},
			false);
	}

	std::int32_t TakeMessage(RE::StaticFunctionTag*, std::int32_t a_token, std::int32_t a_request)
	{
		return Guard([&] { return MCMBridge::NativeFacadeSession().TakeMessage(a_token, a_request); }, std::int32_t{ -2 });
	}

	std::int32_t RequestMessage(RE::StaticFunctionTag*, std::int32_t a_token, RE::BSFixedString a_message,
		RE::BSFixedString a_accept, RE::BSFixedString a_cancel)
	{
		std::int32_t request{};
		const auto   result = Guard([&] {
			auto* tasks = SKSE::GetTaskInterface();
			if (!tasks)
				return std::int32_t{};
			auto arguments = Strings({ a_message, a_accept, a_cancel });
			request = MCMBridge::NativeFacadeSession().BeginMessage(a_token);
			if (!request)
				return request;
			const auto identity = MCMBridge::NativeFacadeSession().ReadIdentity(a_token);
			SKSE::log::info("Native MCM message requested: mod={} token={} request={} cancel={} text={:?}",
				identity ? identity->modID : "unknown", a_token, request, !arguments[2].empty(), arguments[0]);
			// Only dialog presentation crosses to the game queue, never individual controls.
			tasks->AddTask([a_token, request, arguments = std::move(arguments)]() mutable {
				auto valid = [a_token, request] { return MCMBridge::NativeFacadeSession().IsMessageActive(a_token, request); };
				if (!valid())
					return;
				auto complete = [a_token, request](bool a_accepted) {
					const bool delivered = MCMBridge::NativeFacadeSession().CompleteMessage(a_token, request, a_accepted);
					SKSE::log::info("Native MCM message completed: token={} request={} accepted={} delivered={}", a_token, request, a_accepted, delivered);
				};
				const bool shown = Guard([&] { return MCMBridge::MessageDialog::Handle(std::move(arguments), complete, valid); }, false);
				if (!shown)
					complete(false);
			});
			return request;
		},
			std::int32_t{});
		if (!result && request) {
			MCMBridge::NativeFacadeSession().CompleteMessage(a_token, request, false);
			MCMBridge::NativeFacadeSession().TakeMessage(a_token, request);
		}
		return result;
	}
}

namespace MCMBridge
{
	bool SetNativeCustomContent(RE::StaticFunctionTag*, std::int32_t a_token, RE::BSFixedString a_source, float a_x, float a_y)
	{
		return Guard([&] { return NativeFacadeSession().SetCustomContent(a_token, a_source.c_str(), a_x, a_y); }, false);
	}

	bool RegisterNativeFacadeRuntime(RE::BSScript::IVirtualMachine* a_vm)
	{
		if (!a_vm)
			return false;
		NativeFacadeBinding binding(*a_vm);
		binding.RegisterFunction("SetCustomContent", SetNativeCustomContent);
		binding.RegisterFunction("LogError", LogError);
		binding.RegisterFunction("IsActive", IsActive);
		binding.RegisterFunction("BeginDialog", BeginDialog);
		binding.RegisterFunction("FinishDialog", FinishDialog);
		binding.RegisterFunction("SetPresentation", SetPresentation);
		binding.RegisterFunction("ImportBuffers", ImportBuffers);
		binding.RegisterFunction("RequestMessage", RequestMessage);
		binding.RegisterFunction("TakeMessage", TakeMessage);
		return binding.IsValid();
	}
}
