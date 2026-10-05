#include "MCMBridge/Papyrus/NativeHostUI.h"

#include "MCMBridge/Papyrus/NativeFacade.h"

#include <atomic>
#include <unordered_map>

namespace
{
	using NativeBase = RE::BSScript::NF_util::NativeFunctionBase;
	std::atomic_bool ready{};
	struct Owner
	{
		std::int32_t                token{};
		const RE::BSScript::Object* script{};
	};
	std::mutex                                                            ownersMutex;
	std::unordered_map<const RE::BSScript::IStackCallbackFunctor*, Owner> owners;
	struct ScriptOwner
	{
		RE::BSTSmartPointer<RE::BSScript::Stack> stack;
		Owner                                    owner;
	};
	std::unordered_map<const RE::BSScript::Stack*, ScriptOwner> scriptOwners;
	constexpr std::string_view                                  root = "_root.ConfigPanelFader.configPanel.";

	std::optional<std::int32_t> Resolve(const RE::BSScript::StackFrame& a_frame, std::string_view a_menu, std::string_view a_target)
	{
		if (a_menu != "Journal Menu" || !a_frame.parent ||
			(!a_target.starts_with(root) && a_target != "_root.QuestJournalFader.Menu_mc.ConfigPanelClose" &&
				a_target != "_root.QuestJournalFader.Menu_mc.CloseMenu"))
			return std::nullopt;
		const auto  callback = a_frame.parent->callback;
		const auto* caller = a_frame.previousFrame;
		while (caller && !caller->self.IsObject())
			caller = caller->previousFrame;
		if (!caller)
			return std::nullopt;
		const std::scoped_lock lock(ownersMutex);
		if (const auto external = scriptOwners.find(a_frame.parent); external != scriptOwners.end())
			return caller->self.GetObject().get() == external->second.owner.script ? std::optional(external->second.owner.token) : std::nullopt;
		const auto found = owners.find(callback.get());
		if (found == owners.end() || caller->self.GetObject().get() != found->second.script)
			return std::nullopt;
		// Return even an expired token: late owned calls must not touch the Journal.
		return found->second.token;
	}

	template <class T>
	class Read final : public RE::NativeFunction<T(RE::StaticFunctionTag*, RE::BSFixedString, RE::BSFixedString)>
	{
		using Base = RE::NativeFunction<T(RE::StaticFunctionTag*, RE::BSFixedString, RE::BSFixedString)>;

	public:
		Read(std::string_view a_name, RE::BSTSmartPointer<RE::BSScript::IFunction> a_original) :
			Base(a_name, "UI", [](RE::StaticFunctionTag*, RE::BSFixedString, RE::BSFixedString) { return T{}; }), original(std::move(a_original))
		{
			this->SetCallableFromTasklets(original->CanBeCalledFromTasklets());
		}
		bool MarshallAndDispatch(RE::BSScript::Variable& a_base, RE::BSScript::Internal::VirtualMachine& a_vm,
			RE::VMStackID a_stackID, RE::BSScript::Variable& a_result, const RE::BSScript::StackFrame& a_frame) const override
		{
			const auto             page = a_frame.GetPageForFrame();
			const auto             menu = a_frame.GetStackFrameVariable(0, page).Unpack<RE::BSFixedString>();
			const auto             target = a_frame.GetStackFrameVariable(1, page).Unpack<RE::BSFixedString>();
			const std::string_view path(target.c_str());
			const auto             token = Resolve(a_frame, menu.c_str(), path);
			if constexpr (std::is_same_v<T, RE::BSFixedString>) {
				if (!token && MCMBridge::NativeHostUI::InterceptExternalPage(a_frame, menu.c_str(), path, a_result))
					return true;
				if (!token && MCMBridge::NativeHostUI::InterceptExternalState(a_frame, menu.c_str(), path, a_result))
					return true;
				if (!token && MCMBridge::NativeHostUI::InterceptExternalClose(a_frame, menu.c_str(), path, a_result))
					return true;
			}
			const auto field = path.starts_with(root) ? path.substr(root.size()) : std::string_view{};
			if (token) {
				const auto value = MCMBridge::NativeFacadeSession().ReadOptionCursor(*token);
				if constexpr (std::is_same_v<T, std::int32_t>) {
					if (field == "optionCursor.optionType") {
						a_result.SetSInt(value ? value->type : 0);
						return true;
					}
				} else if constexpr (std::is_same_v<T, float>) {
					if (field == "optionCursor.numValue") {
						a_result.SetFloat(value ? value->value : 0.0F);
						return true;
					}
				} else if (field == "contentHolder.background._url") {
					// The native frontend has no Journal background movie. Library
					// theme probes must not read a separately opened Journal.
					a_result.SetString(RE::BSFixedString(""));
					return true;
				} else if (field == "contentHolder.modListPanel.decorTitle.textHolder.textField.text") {
					RE::BSFixedString name("");
					if (MCMBridge::NativeFacadeSession().IsActive(*token)) {
						const auto* caller = a_frame.previousFrame;
						while (caller && !caller->self.IsObject()) caller = caller->previousFrame;
						const auto* property = caller ? caller->self.GetObject()->GetProperty("ModName") : nullptr;
						if (property && property->IsString())
							name = property->GetString();
					}
					a_result.SetString(name);
					return true;
				}
			}
			return static_cast<NativeBase*>(original.get())->MarshallAndDispatch(a_base, a_vm, a_stackID, a_result, a_frame);
		}

	private:
		RE::BSTSmartPointer<RE::BSScript::IFunction> original;
	};

	RE::BSTSmartPointer<RE::BSScript::IFunction> Find(RE::BSScript::ObjectTypeInfo& a_type, std::string_view a_name)
	{
		if (a_type.IsLinked()) {
			const auto* functions = a_type.GetGlobalFuncIter();
			for (std::uint32_t index = 0; index < a_type.GetNumGlobalFuncs(); ++index)
				if (functions[index].func && functions[index].func->GetName() == a_name)
					return functions[index].func;
		} else {
			for (const auto* entry = a_type.GetUnlinkedFunctionIter(); entry; entry = entry->next)
				if (entry->func && entry->func->GetName() == a_name)
					return entry->func;
		}
		return {};
	}

	template <class T>
	bool Bind(RE::BSScript::IVirtualMachine& a_vm, RE::BSScript::ObjectTypeInfo& a_type, std::string_view a_name)
	{
		auto original = Find(a_type, a_name);
		if (!original || !original->GetIsNative() || !original->GetIsStatic() || original->GetParamCount() != 2 ||
			static_cast<NativeBase*>(original.get())->GetIsLatent())
			return false;
		RE::BSTSmartPointer<RE::BSScript::IFunction> replacement{ new Read<T>(a_name, original) };
		if (replacement->GetReturnType() != original->GetReturnType())
			return false;
		for (std::uint32_t index = 0; index < 2; ++index) {
			RE::BSFixedString      name;
			RE::BSScript::TypeInfo expected;
			RE::BSScript::TypeInfo actual;
			replacement->GetParam(index, name, expected);
			original->GetParam(index, name, actual);
			if (expected != actual)
				return false;
		}
		return a_vm.BindNativeMethod(replacement.get());
	}
}

namespace MCMBridge::NativeHostUI
{
	std::optional<std::int32_t> ResolveSession(const RE::BSScript::StackFrame& a_frame, std::string_view a_menu)
	{
		return Resolve(a_frame, a_menu, root);
	}
	std::optional<std::int32_t> ResolveCallbackSession(const RE::BSScript::StackFrame& a_frame)
	{
		if (!a_frame.parent)
			return std::nullopt;
		const std::scoped_lock lock(ownersMutex);
		if (const auto external = scriptOwners.find(a_frame.parent); external != scriptOwners.end())
			return external->second.owner.token;
		const auto found = owners.find(a_frame.parent->callback.get());
		return found == owners.end() ? std::nullopt : std::optional(found->second.token);
	}
	void AttachScriptStack(RE::BSTSmartPointer<RE::BSScript::Stack> a_stack, std::int32_t a_token, const RE::BSScript::Object* a_owner)
	{
		const std::scoped_lock lock(ownersMutex);
		const auto*            key = a_stack.get();
		scriptOwners.insert_or_assign(key, ScriptOwner{ std::move(a_stack), { a_token, a_owner } });
	}
	void DetachScriptStack(const RE::BSScript::Stack* a_stack)
	{
		const std::scoped_lock lock(ownersMutex);
		scriptOwners.erase(a_stack);
	}
	bool IsScriptStack(const RE::BSScript::StackFrame& a_frame)
	{
		const std::scoped_lock lock(ownersMutex);
		return scriptOwners.contains(a_frame.parent);
	}
	void CollectScriptStacks()
	{
		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		if (!vm)
			return;
		std::vector<RE::BSTSmartPointer<RE::BSScript::Stack>> candidates;
		{
			const std::scoped_lock lock(ownersMutex);
			for (const auto& [key, owner] : scriptOwners) {
				(void)key;
				candidates.push_back(owner.stack);
			}
		}
		for (const auto& stack : candidates) {
			bool running{};
			{
				const RE::BSSpinLockGuard lock(vm->runningStacksLock);
				const auto                found = vm->allRunningStacks.find(stack->stackID);
				running = found != vm->allRunningStacks.end() && found->second.get() == stack.get();
			}
			// Never inspect mutable frames or acquire our mutex under the VM lock.
			if (!running)
				DetachScriptStack(stack.get());
		}
	}
	void Attach(const RE::BSScript::IStackCallbackFunctor* a_callback, std::int32_t a_token, const RE::BSScript::Object* a_owner)
	{
		const std::scoped_lock lock(ownersMutex);
		owners.insert_or_assign(a_callback, Owner{ a_token, a_owner });
	}
	void Detach(const RE::BSScript::IStackCallbackFunctor* a_callback)
	{
		const std::scoped_lock lock(ownersMutex);
		owners.erase(a_callback);
	}
	bool SetCursor(const RE::BSScript::StackFrame& a_frame, std::string_view a_menu, std::string_view a_target, std::int32_t a_slot)
	{
		if (a_target != "_root.ConfigPanelFader.configPanel.optionCursorIndex")
			return false;
		const auto token = Resolve(a_frame, a_menu, a_target);
		if (!token)
			return false;
		NativeFacadeSession().SetOptionCursor(*token, a_slot);
		return true;
	}
	bool IsReady() { return ready.load(); }
	bool Focus(const RE::BSScript::StackFrame& a_frame, std::string_view a_menu, std::string_view a_target)
	{
		return a_target == "_root.ConfigPanelFader.configPanel.changeFocus" && Resolve(a_frame, a_menu, a_target).has_value();
	}
	bool Close(const RE::BSScript::StackFrame& a_frame, std::string_view a_menu, std::string_view a_target, bool a_close)
	{
		const bool frontend = a_target == "_root.QuestJournalFader.Menu_mc.CloseMenu";
		if (!frontend && a_target != "_root.ConfigPanelFader.configPanel.contentHolder.modListPanel.showList" &&
			a_target != "_root.QuestJournalFader.Menu_mc.ConfigPanelClose")
			return false;
		const auto token = Resolve(a_frame, a_menu, a_target);
		if (!token)
			return false;
		// UI.Invoke delegates to InvokeBool with false in the SKSE script.
		if (!frontend || a_close)
			NativeFacadeSession().RequestClose(*token, frontend);
		return true;
	}
	bool Unlock(const RE::BSScript::StackFrame& a_frame, std::string_view a_menu, std::string_view a_target)
	{
		return a_target == "_root.ConfigPanelFader.configPanel.unlock" && Resolve(a_frame, a_menu, a_target).has_value();
	}
	bool SelectPage(const RE::BSScript::StackFrame& a_frame, std::string_view a_menu, std::string_view a_target, const std::vector<std::int32_t>& a_values)
	{
		constexpr std::string_view list = "_root.ConfigPanelFader.configPanel.contentHolder.modListPanel.subListFader.list.";
		if (!a_target.starts_with(list))
			return false;
		const auto method = a_target.substr(list.size());
		if (method != "onItemPress" && method != "doSetSelectedIndex")
			return false;
		const auto token = Resolve(a_frame, a_menu, a_target);
		if (!token)
			return false;
		if (method != "onItemPress" || a_values.size() != 2 || a_values[0] < 0 || !NativeFacadeSession().IsActive(*token))
			return true;
		auto pages = NativeFacadeSession().ReadNavigation(*token);
		if (!pages) {
			const auto* caller = a_frame.previousFrame;
			while (caller && !caller->self.IsObject()) caller = caller->previousFrame;
			const auto* property = caller ? caller->self.GetObject()->GetProperty("Pages") : nullptr;
			const auto  array = property && property->IsArray() ? property->GetArray() : nullptr;
			if (!array)
				return true;
			pages.emplace();
			for (const auto& value : *array) {
				if (!value.IsString())
					return true;
				pages->emplace_back(value.GetString());
			}
		}
		const auto index = static_cast<std::size_t>(a_values[0]);
		if (index < pages->size())
			NativeFacadeSession().RequestPage(*token, (*pages)[index], a_values[0]);
		return true;
	}
	bool SetNavigation(const RE::BSScript::StackFrame& a_frame, std::string_view a_menu, std::string_view a_target, const std::vector<RE::BSFixedString>& a_pages)
	{
		if (a_target != "_root.ConfigPanelFader.configPanel.setPageNames")
			return false;
		const auto token = Resolve(a_frame, a_menu, a_target);
		if (!token)
			return InterceptExternalNavigation(a_frame, a_menu, a_target);
		std::vector<std::string> pages;
		pages.reserve(a_pages.size());
		for (const auto& page : a_pages)
			pages.emplace_back(page.c_str());
		NativeFacadeSession().SetNavigation(*token, std::move(pages));
		return true;
	}
	bool Register(RE::BSScript::IVirtualMachine& a_vm, RE::BSScript::ObjectTypeInfo& a_type, bool a_cursorReady)
	{
		const auto integers = Bind<std::int32_t>(a_vm, a_type, "GetInt");
		const auto numbers = Bind<float>(a_vm, a_type, "GetFloat");
		const auto names = Bind<RE::BSFixedString>(a_vm, a_type, "GetString");
		const auto menuState = RegisterMenuState(a_vm, a_type);
		const auto quickOpen = RegisterQuickOpen(a_vm);
		const auto menuMode = RegisterMenuMode(a_vm);
		SKSE::log::info("Native host scoped cursor reads: type={} value={}", integers, numbers);
		SKSE::log::info("Native host scoped menu-state binding: {}", menuState);
		SKSE::log::info("Native host quick-open binding: {}", quickOpen);
		SKSE::log::info("Native host scoped menu-mode binding: {}", menuMode);
		ready.store(integers && numbers && names && menuState && quickOpen && a_cursorReady && menuMode);
		return ready.load();
	}
}
