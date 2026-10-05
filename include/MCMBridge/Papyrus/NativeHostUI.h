#pragma once

#include "RE/Skyrim.h"

#include <optional>

namespace MCMBridge::NativeHostUI
{
	// The callback owns this registration until the VM releases its last reference.
	void Attach(const RE::BSScript::IStackCallbackFunctor* a_callback, std::int32_t a_token, const RE::BSScript::Object* a_owner);
	void Detach(const RE::BSScript::IStackCallbackFunctor* a_callback);
	void AttachScriptStack(RE::BSTSmartPointer<RE::BSScript::Stack> a_stack, std::int32_t a_token, const RE::BSScript::Object* a_owner);
	void DetachScriptStack(const RE::BSScript::Stack* a_stack);
	bool IsScriptStack(const RE::BSScript::StackFrame& a_frame);
	void CollectScriptStacks();
	bool SetCursor(const RE::BSScript::StackFrame& a_frame, std::string_view a_menu, std::string_view a_target, std::int32_t a_slot);
	bool SetNavigation(const RE::BSScript::StackFrame& a_frame, std::string_view a_menu, std::string_view a_target, const std::vector<RE::BSFixedString>& a_pages);
	bool Register(RE::BSScript::IVirtualMachine& a_vm, RE::BSScript::ObjectTypeInfo& a_type, bool a_cursorReady);
	bool IsReady();
	// Unlike UI routing, expiry admission follows the callback through delegated scripts.
	std::optional<std::int32_t> ResolveCallbackSession(const RE::BSScript::StackFrame& a_frame);
	bool                        InterceptExternalNavigation(const RE::BSScript::StackFrame& a_frame, std::string_view a_menu, std::string_view a_target);
	bool                        InterceptExternalPage(const RE::BSScript::StackFrame& a_frame, std::string_view a_menu, std::string_view a_target, RE::BSScript::Variable& a_result);
	bool                        InterceptExternalState(const RE::BSScript::StackFrame& a_frame, std::string_view a_menu, std::string_view a_target, RE::BSScript::Variable& a_result);
	bool                        InterceptExternalClose(const RE::BSScript::StackFrame& a_frame, std::string_view a_menu, std::string_view a_target, RE::BSScript::Variable& a_result);
	bool                        RegisterQuickOpen(RE::BSScript::IVirtualMachine& a_vm);
	bool                        RegisterMenuMode(RE::BSScript::IVirtualMachine& a_vm);
	// A missing owner preserves the original UI call. An expired owner is still
	// returned so a late callback cannot observe or operate the real Journal.
	std::optional<std::int32_t> ResolveSession(const RE::BSScript::StackFrame& a_frame, std::string_view a_menu);
	bool                        RegisterMenuState(RE::BSScript::IVirtualMachine& a_vm, RE::BSScript::ObjectTypeInfo& a_type);
	bool                        Focus(const RE::BSScript::StackFrame& a_frame, std::string_view a_menu, std::string_view a_target);
	bool                        Close(const RE::BSScript::StackFrame& a_frame, std::string_view a_menu, std::string_view a_target, bool a_close);
	bool                        SelectPage(const RE::BSScript::StackFrame& a_frame, std::string_view a_menu, std::string_view a_target, const std::vector<std::int32_t>& a_values);
	bool                        Unlock(const RE::BSScript::StackFrame& a_frame, std::string_view a_menu, std::string_view a_target);
}
