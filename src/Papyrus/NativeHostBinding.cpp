#include "MCMBridge/Papyrus/NativeHostBinding.h"

#include "MCMBridge/Papyrus/NativeFacade.h"
#include "MCMBridge/Papyrus/NativeHostUI.h"
#include "MCMBridge/Plugin/BridgeController.h"
#include "MCMBridge/Plugin/GamePauseMenu.h"

namespace
{
	void TraceOutstandingCall(const RE::BSScript::IStackCallbackFunctor* a_callback, std::string_view a_modID)
	{
		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		if (!vm || !a_callback)
			return;
		std::optional<RE::VMStackID> stackID;
		std::vector<RE::VMStackID>   otherStacks;
		{
			const RE::BSSpinLockGuard lock(vm->runningStacksLock);
			for (const auto& [id, stack] : vm->allRunningStacks) {
				if (stack && stack->callback.get() == a_callback) {
					stackID = id;
				} else if (stack) {
					otherStacks.push_back(id);
				}
			}
		}
		// The engine traces its own frames. Do not walk mutable VM frames from the
		// game thread or call TraceStack while holding the stack registry lock.
		if (stackID) {
			SKSE::log::warn("Native MCM timeout stack: mod={} stack={} trace_requested_in=Papyrus.log", a_modID, *stackID);
			vm->TraceStack("MCMBridge: outstanding native host call", *stackID);
			// A cross-object call can wait for an unrelated stack holding the target.
			// Snapshot IDs only; let the engine inspect frames after releasing the lock.
			// Bound failure-only output and report truncation rather than implying completeness.
			constexpr std::size_t maxContextStacks = 256;
			std::ranges::sort(otherStacks);
			const auto count = std::min(otherStacks.size(), maxContextStacks);
			auto*      ui = RE::UI::GetSingleton();
			SKSE::log::warn("Native MCM timeout context: mod={} game_paused={} bridge_pause={} journal_open={} stacks={} traced={} truncated={}",
				a_modID, ui && ui->GameIsPaused(), MCMBridge::GamePauseMenu::IsOpen(),
				MCMBridge::GamePauseMenu::IsJournalOpen(), otherStacks.size(), count, otherStacks.size() > count);
			for (std::size_t index = 0; index < count; ++index) {
				const auto message = std::format("MCMBridge: timeout context for stack {}, peer stack {}", *stackID, otherStacks[index]);
				vm->TraceStack(message.c_str(), otherStacks[index]);
			}
		} else {
			SKSE::log::warn("Native MCM timeout stack: mod={} callback stack no longer registered", a_modID);
		}
	}

	std::optional<MCMBridge::ClassicPageBuffers> ReadBuffers(RE::BSScript::Object& a_script)
	{
		MCMBridge::ClassicPageBuffers buffers;
		const std::array              names{ "_optionFlagsBuf", "_textBuf", "_strValueBuf", "_numValueBuf", "_stateOptionMap" };
		for (std::size_t column = 0; column < names.size(); ++column) {
			const auto* variable = a_script.GetVariable(RE::BSFixedString(names[column]));
			const auto  values = variable && variable->IsArray() ? variable->GetArray() : nullptr;
			if (!values || values->size() != 128)
				return std::nullopt;
			for (const auto& value : *values) {
				if (column == 0) {
					if (!value.IsInt())
						return std::nullopt;
					buffers.optionFlags.push_back(value.GetSInt());
				} else if (column == 3) {
					if (!value.IsFloat())
						return std::nullopt;
					buffers.numericValues.push_back(value.GetFloat());
				} else {
					if (!value.IsString())
						return std::nullopt;
					auto& strings = column == 1 ? buffers.labels : column == 2 ? buffers.stringValues :
					                                                             buffers.stateNames;
					strings.emplace_back(value.GetString());
				}
			}
		}
		return buffers;
	}

	class OwnedCompletion final : public RE::BSScript::IStackCallbackFunctor
	{
	public:
		explicit OwnedCompletion(std::function<void(bool)> a_receiver) : receiver(std::move(a_receiver)) {}
		~OwnedCompletion() override { MCMBridge::NativeHostUI::Detach(this); }
		void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}
		void operator()(RE::BSScript::Variable a_result) override
		{
			if (auto* tasks = SKSE::GetTaskInterface()) {
				const bool accepted = a_result.IsBool() && a_result.GetBool();
				tasks->AddTask([receiver = std::move(receiver), accepted] { if (receiver) receiver(accepted); });
			}
		}

	private:
		std::function<void(bool)> receiver;
	};

	RE::BSScript::IFunctionArguments* Arguments(MCMBridge::ClassicCall a_call)
	{
		using Method = MCMBridge::ClassicMethod;
		switch (a_call.method) {
		case Method::kOpenConfig:
		case Method::kCloseConfig:
			return RE::MakeFunctionArguments();
		case Method::kSetPage:
		case Method::kSetModSettingInt:
			return RE::MakeFunctionArguments(std::move(a_call.text), std::int32_t(a_call.integer));
		case Method::kSetSliderValue:
			return RE::MakeFunctionArguments(float(a_call.number));
		case Method::kSetInputText:
		case Method::kOnSettingChange:
			return RE::MakeFunctionArguments(std::move(a_call.text));
		case Method::kRemapKey:
			return RE::MakeFunctionArguments(std::int32_t(a_call.integer), std::int32_t(a_call.secondaryInteger), std::move(a_call.text), std::move(a_call.secondaryText));
		default:
			return RE::MakeFunctionArguments(std::int32_t(a_call.integer));
		}
	}
}

namespace MCMBridge
{
	struct NativeHostBinding::State : std::enable_shared_from_this<State>
	{
		RE::BSTSmartPointer<RE::BSScript::Object>                script;
		std::uint64_t                                            session{};
		std::string                                              modID;
		std::int32_t                                             token{};
		bool                                                     busy{};
		bool                                                     hasPage{};
		bool                                                     retired{};
		bool                                                     redirected{};
		ClassicMethod                                            pendingMethod{ ClassicMethod::kOpenConfig };
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> pendingCallback;

		bool Active() const { return NativeFacadeSession().IsActive(token); }
		bool Send(ClassicCall a_call, IClassicScript::Continuation a_next, bool a_allowPageRequest = true)
		{
			auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			if (!vm || !Active())
				return false;
			// A previous UI override must not mask later direct changes to Pages.
			NativeFacadeSession().ClearNavigationOverride(token);
			RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> callback;
			callback.reset(new OwnedCompletion([weak = weak_from_this(), expectedToken = token, method = a_call.method, next = std::move(a_next), a_allowPageRequest,
												   viewRevision = BridgeController::GetSingleton().ScriptViewRevision(session, modID)](bool) mutable {
				if (const auto owner = weak.lock(); owner && owner->Active() && owner->token == expectedToken) {
					owner->pendingCallback.reset();
					const auto close = NativeFacadeSession().TakeCloseRequest(expectedToken);
					// Helper can update the mirrored buffers without calling facade setters.
					// Closed buffers are historical and must not be used as confirmation.
					if (method != ClassicMethod::kCloseConfig) {
						auto buffers = ReadBuffers(*owner->script);
						if (!buffers || !NativeFacadeSession().ImportBuffers(owner->token, std::move(*buffers))) {
							owner->hasPage = false;
							NativeFacadeSession().Close(owner->token);
							SKSE::log::error("Native MCM callback produced invalid compatibility buffers");
						} else {
							// Helper writes _infoText directly, including forceUpdate=false.
							const auto* info = owner->script->GetVariable("_infoText");
							if (info && info->IsString())
								NativeFacadeSession().SetPresentation(owner->token, 1, std::string(info->GetString()));
							owner->hasPage = NativeFacadeSession().Publish(owner->token);
						}
					} else {
						owner->hasPage = false;
						BridgeController::GetSingleton().NativeRegistry().ClearActive(owner->script);
						// Release before the continuation can open another MCM. A later
						// confirmation opens a fresh token and rebinds this facade.
						NativeFacadeSession().Close(owner->token);
						owner->token = 0;
					}
					if (auto request = NativeFacadeSession().TakePageRequest(expectedToken); request && owner->hasPage) {
						// The original callback has finished. Follow at most one requested page;
						// recursive UI navigation must not create an unbounded VM dispatch loop.
						if (a_allowPageRequest) {
							const auto* property = owner->script->GetProperty("Pages");
							const auto  pages = property && property->IsArray() ? property->GetArray() : nullptr;
							const auto  index = static_cast<std::uint32_t>(request->index);
							if (pages && index < pages->size() && (*pages)[index].IsString() && request->name == (*pages)[index].GetString()) {
								ClassicCall selection{ ClassicMethod::kSetPage };
								selection.integer = request->index;
								selection.text = std::move(request->name);
								if (!owner->Send(std::move(selection), std::move(next), false))
									SKSE::log::error("Native page selection dispatch failed; awaiting sequencer timeout");
								return;
							}
						}
						NativeFacadeSession().SetPresentation(expectedToken, 4, {});
						NativeFacadeSession().Publish(expectedToken);
						SKSE::log::warn("Native page selection rejected: recursive request or changed page identity");
					}
					owner->busy = false;
					owner->redirected = !a_allowPageRequest;
					if (close && viewRevision)
						BridgeController::GetSingleton().CloseScriptView(owner->session, *viewRevision, owner->modID, *close);
					if (next)
						next();
				}
			}));
			NativeHostUI::Attach(callback.get(), token, script.get());
			pendingCallback = callback;
			const auto method = RE::BSFixedString(ClassicMethodName(a_call.method));
			return vm->DispatchMethodCall(script, method, Arguments(std::move(a_call)), callback);
		}
	};

	NativeHostBinding::NativeHostBinding(std::shared_ptr<State> a_state) : state(std::move(a_state)) {}
	NativeHostBinding::~NativeHostBinding() { Invalidate(); }
	void NativeHostBinding::Invalidate()
	{
		if (state) {
			if (state->busy) {
				SKSE::log::warn("Native MCM outstanding call retired: mod={} token={} method={} stage=callback has_page={}",
					state->modID, state->token, ClassicMethodName(state->pendingMethod), state->hasPage);
				TraceOutstandingCall(state->pendingCallback.get(), state->modID);
			}
			state->pendingCallback.reset();
			auto& registry = BridgeController::GetSingleton().NativeRegistry();
			if (state->token && state->Active() && registry.Session() == state->session)
				registry.ClearActive(state->script);
			NativeFacadeSession().Close(state->token);
			state->token = 0;
			state->busy = false;
			state->hasPage = false;
			state->retired = true;
		}
	}
	std::int32_t                        NativeHostBinding::Token() const { return state->token; }
	std::optional<ClassicPageSelection> NativeHostBinding::TakePageRedirect()
	{
		if (!HasPage() || !std::exchange(state->redirected, false))
			return std::nullopt;
		const auto page = NativeFacadeSession().Read();
		return page ? std::optional(ClassicPageSelection{ page->page, page->index }) : std::nullopt;
	}
	bool NativeHostBinding::HasPage() const { return state->Active() && state->hasPage && !state->busy; }

	Result<std::unique_ptr<NativeHostBinding>> NativeHostBinding::Create(std::uint64_t a_session, std::string a_modID,
		RE::BSTSmartPointer<RE::BSScript::Object> a_script)
	{
		if (!HasNativeFacadeContract(a_script))
			return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "Native MCM facade contract is missing or incompatible" });
		if (!a_session || NativeFacadeSession().Session() != a_session)
			return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "Native MCM adapter belongs to an expired session" });
		auto owner = std::make_shared<State>();
		owner->script = std::move(a_script);
		owner->session = a_session;
		owner->modID = a_modID;
		// Read-only navigation adapters do not acquire the execution owner.
		return std::unique_ptr<NativeHostBinding>(new NativeHostBinding(std::move(owner)));
	}

	bool NativeHostBinding::Dispatch(ClassicCall a_call, IClassicScript::Continuation a_continuation)
	{
		if (state->retired)
			return false;
		if (!state->token && a_call.method == ClassicMethod::kOpenConfig) {
			auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			auto* policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
			auto* form = policy ? policy->GetObjectForHandle(RE::FormType::Quest, state->script->GetHandle()) : nullptr;
			auto* quest = form ? form->As<RE::TESQuest>() : nullptr;
			SKSE::log::info("Native MCM open admission: mod={} script_initialized={} quest_present={} quest_running={} quest_starting={} quest_stopping={}",
				state->modID, state->script->IsInitialized(), quest != nullptr,
				quest && quest->IsRunning(), quest && quest->IsStarting(), quest && quest->IsStopping());
			auto token = NativeFacadeSession().Open(state->session, state->modID, reinterpret_cast<std::uintptr_t>(state->script.get()));
			if (!token)
				return false;
			state->token = *token;
			if (!BridgeController::GetSingleton().NativeRegistry().SetActive(state->script)) {
				NativeFacadeSession().Close(state->token);
				state->token = 0;
				return false;
			}
		}
		if (state->busy || !state->Active())
			return false;
		state->busy = true;
		state->pendingMethod = a_call.method;
		state->redirected = false;
		const bool sent = state->Send(std::move(a_call), std::move(a_continuation));
		if (!sent)
			state->busy = false;
		return sent;
	}
}
