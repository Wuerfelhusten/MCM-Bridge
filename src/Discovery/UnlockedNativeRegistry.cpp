#include "MCMBridge/Discovery/UnlockedNativeRegistry.h"
#include "MCMBridge/Core/UnlockedVersion.h"

namespace
{
	using Callback = std::function<void(MCMBridge::Result<RE::BSScript::Variable>)>;

	class NativeResult final : public RE::BSScript::IStackCallbackFunctor
	{
	public:
		explicit NativeResult(Callback a_callback) : callback(std::move(a_callback)) {}
		void operator()(RE::BSScript::Variable a_result) override
		{
			if (auto* tasks = SKSE::GetTaskInterface()) {
				tasks->AddTask([callback = std::move(callback), result = std::move(a_result)]() mutable {
					if (callback)
						callback(std::move(result));
				});
			}
		}
		void SetObject(const RE::BSTSmartPointer<RE::BSScript::Object>&) override {}

	private:
		Callback callback;
	};

	template <class... Args>
	void Call(const char* a_name, Callback a_callback, Args... a_args)
	{
		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		if (!vm) {
			a_callback(std::unexpected(MCMBridge::BridgeError{ MCMBridge::BridgeErrorCode::kUnavailable, "Papyrus VM unavailable" }));
			return;
		}
		RE::BSTSmartPointer<RE::BSScript::IStackCallbackFunctor> result(new NativeResult(a_callback));
		if (!vm->DispatchStaticCall("MCMUnlocked", a_name, RE::MakeFunctionArguments(std::move(a_args)...), result)) {
			a_callback(std::unexpected(MCMBridge::BridgeError{ MCMBridge::BridgeErrorCode::kDispatchFailed,
				std::format("MCM Unlocked native call failed: {}", a_name) }));
		}
	}

	MCMBridge::BridgeError InvalidResult()
	{
		return { MCMBridge::BridgeErrorCode::kInvalidData, "MCM Unlocked returned an invalid registry value" };
	}
}

namespace MCMBridge
{
	void ReadUnlockedRegistryCount(std::function<void(Result<std::int32_t>)> a_done)
	{
		Call("GetConfigCount", [done = std::move(a_done)](Result<RE::BSScript::Variable> a_value) {
			if (!a_value)
				return done(std::unexpected(a_value.error()));
			if (!a_value->IsInt() || a_value->GetSInt() < 0)
				return done(std::unexpected(InvalidResult()));
			done(a_value->GetSInt());
		});
	}

	std::shared_ptr<UnlockedRegistryQuery> CreateUnlockedRegistryQuery()
	{
		auto count = [](std::function<void(Result<std::int32_t>)> a_done) {
			Call("GetVersion", [done = std::move(a_done)](Result<RE::BSScript::Variable> a_version) {
				if (!a_version)
					return done(std::unexpected(a_version.error()));
				const auto values = a_version->IsArray() ? a_version->GetArray() : nullptr;
				if (!values || values->size() != 3 || !(*values)[0].IsInt() || !(*values)[1].IsInt() ||
					!(*values)[2].IsInt()) {
					return done(std::unexpected(InvalidResult()));
				}
				if (!SupportsUnlockedVersion({ (*values)[0].GetSInt(), (*values)[1].GetSInt(), (*values)[2].GetSInt() })) {
					return done(std::unexpected(BridgeError{ BridgeErrorCode::kUnsupported,
						std::format("MCM Unlocked {}.{}.{} requires an update to 2.1.5 or newer", (*values)[0].GetSInt(), (*values)[1].GetSInt(), (*values)[2].GetSInt()) }));
				}
				Call("GetConfigCount", [done](Result<RE::BSScript::Variable> a_value) {
					if (!a_value)
						return done(std::unexpected(a_value.error()));
					if (!a_value->IsInt())
						return done(std::unexpected(InvalidResult()));
					done(a_value->GetSInt());
				});
			});
		};
		auto entry = [](std::int32_t a_index, std::function<void(Result<UnlockedRegistryEntry>)> a_done) {
			Call("GetModIDFromConfigID", [done = std::move(a_done)](Result<RE::BSScript::Variable> a_value) {
                if (!a_value) return done(std::unexpected(a_value.error()));
                if (!a_value->IsString() || a_value->GetString().empty()) return done(std::unexpected(InvalidResult()));
                std::string id(a_value->GetString());
                Call("GetMarkerFromModID", [id, done](Result<RE::BSScript::Variable> a_marker) {
                    if (!a_marker) return done(std::unexpected(a_marker.error()));
                    if (!a_marker->IsObject() || !a_marker->GetObject()) return done(std::unexpected(InvalidResult()));
                    const auto handle = a_marker->GetObject()->GetHandle();
                    Call("GetModNameFromModID", [id, handle, done](Result<RE::BSScript::Variable> a_name) {
                        if (!a_name) return done(std::unexpected(a_name.error()));
                        if (!a_name->IsString()) return done(std::unexpected(InvalidResult()));
                        done(UnlockedRegistryEntry{ id, std::string(a_name->GetString()), handle });
                    }, id);
                }, id); }, a_index);
		};
		return std::make_shared<UnlockedRegistryQuery>(std::move(count), std::move(entry), UnlockedRegistryQuery::Clock::now);
	}
}
