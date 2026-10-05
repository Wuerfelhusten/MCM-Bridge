#pragma once

#include "MCMBridge/Papyrus/IClassicScript.h"
#include "RE/Skyrim.h"

namespace MCMBridge
{
	// The sequencer owns this binding on the game thread. It must invalidate the
	// binding on timeout or session change; invalidation does not undo mod callbacks.
	class NativeHostBinding
	{
	public:
		static Result<std::unique_ptr<NativeHostBinding>> Create(std::uint64_t a_session, std::string a_modID,
			RE::BSTSmartPointer<RE::BSScript::Object> a_script);
		~NativeHostBinding();
		bool                                Dispatch(ClassicCall a_call, IClassicScript::Continuation a_continuation);
		void                                Invalidate();
		std::int32_t                        Token() const;
		bool                                HasPage() const;
		std::optional<ClassicPageSelection> TakePageRedirect();

	private:
		struct State;
		explicit NativeHostBinding(std::shared_ptr<State> a_state);
		std::shared_ptr<State> state;
	};
}
