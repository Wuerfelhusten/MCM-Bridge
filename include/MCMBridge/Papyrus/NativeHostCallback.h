#pragma once

#include "MCMBridge/Core/HostControlCapture.h"
#include "MCMBridge/Core/HostPageCapture.h"

#include "RE/Skyrim.h"

#include <chrono>
#include <functional>
#include <mutex>

namespace MCMBridge
{
	struct NativeHostObservation
	{
		std::shared_ptr<const HostCapturedPage> page;
		HostControlChanges                      changes;
		std::chrono::steady_clock::duration     dispatchTime{};
		std::chrono::steady_clock::duration     taskWait{};
	};

	// The VM owns the callback and its buffers until completion. Delivery occurs
	// on the game task queue; the receiver must still check its operation token.
	class NativeHostCallback final : public RE::BSScript::IStackCallbackFunctor
	{
	public:
		using Receiver = std::function<void(NativeHostObservation)>;
		NativeHostCallback(RE::BSTSmartPointer<RE::BSScript::Object> a_script, std::string a_menu, std::string a_root, Receiver a_receiver);
		~NativeHostCallback() override;
		static NativeHostCallback* Find(const RE::BSScript::IStackCallbackFunctor* a_callback);
		void                       operator()(RE::BSScript::Variable) override;
		void                       SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
		bool                       Observe(const RE::BSScript::StackFrame& a_frame, std::string_view a_menu, std::string_view a_target, HostBufferPayload a_payload, HostProtocolCallKind a_kind);

	private:
		RE::BSTSmartPointer<RE::BSScript::Object> script;
		std::string                               menu;
		std::string                               root;
		Receiver                                  receiver;
		std::mutex                                mutex;
		HostPageCapture                           capture;
		HostControlCapture                        controls;
		HostCaptureToken                          token;
		std::chrono::steady_clock::time_point     started{ std::chrono::steady_clock::now() };
	};
}
