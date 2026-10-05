#include "MCMBridge/Discovery/LiveMCMFactory.h"

#include "MCMBridge/Core/StableId.h"
#include "MCMBridge/Papyrus/MCMScript.h"

namespace
{
	std::optional<std::string> ReadModName(const RE::BSTSmartPointer<RE::BSScript::Object>& a_script);

	class SkyUIHostAdapter final : public MCMBridge::IMCMHostAdapter
	{
	public:
		SkyUIHostAdapter(RE::BSTSmartPointer<RE::BSScript::Object> a_script, std::string a_name) :
			script(std::move(a_script)), name(std::move(a_name)) {}

		std::shared_ptr<MCMBridge::IClassicScript> CreateSession() const override
		{
			return std::make_shared<MCMBridge::MCMScript>(script);
		}
		bool IsValid() const override
		{
			auto*                                     vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
			RE::BSTSmartPointer<RE::BSScript::Object> bound;
			return vm && script && script->GetTypeInfo() && ReadModName(script) == name &&
			       vm->FindBoundObject(script->GetHandle(), script->GetTypeInfo()->GetName(), bound) && bound.get() == script.get();
		}

	private:
		RE::BSTSmartPointer<RE::BSScript::Object> script;
		std::string                               name;
	};

	std::string ReadScriptName(const RE::BSTSmartPointer<RE::BSScript::Object>& a_script)
	{
		const auto* type = a_script ? a_script->GetTypeInfo() : nullptr;
		const auto* name = type ? type->GetName() : nullptr;
		return name && name[0] ? std::string(name) : std::string{};
	}

	std::optional<std::string> ReadModName(const RE::BSTSmartPointer<RE::BSScript::Object>& a_script)
	{
		if (!a_script) {
			return std::nullopt;
		}
		const auto* value = a_script->GetProperty(RE::BSFixedString("ModName"));
		if (!value || !value->IsString()) {
			value = a_script->GetVariable(RE::BSFixedString("::ModName_var"));
		}
		return value && value->IsString() && !value->GetString().empty() ?
		           std::optional<std::string>(std::string(value->GetString())) :
		           std::nullopt;
	}

	bool IsBasedOn(const RE::BSTSmartPointer<RE::BSScript::Object>& a_script, std::string_view a_baseName)
	{
		for (auto* type = a_script ? a_script->GetTypeInfo() : nullptr; type; type = type->GetParent()) {
			const auto* name = type->GetName();
			if (name && a_baseName == name) {
				return true;
			}
		}
		return false;
	}

	RE::TESQuest* ResolveQuest(const RE::BSTSmartPointer<RE::BSScript::Object>& a_script)
	{
		auto* vm = RE::BSScript::Internal::VirtualMachine::GetSingleton();
		auto* policy = vm ? vm->GetObjectHandlePolicy() : nullptr;
		auto* form = policy && a_script ? policy->GetObjectForHandle(RE::FormType::Quest, a_script->GetHandle()) : nullptr;
		return form ? form->As<RE::TESQuest>() : nullptr;
	}

	std::string ReadOwnerPlugin(const RE::TESQuest* a_quest)
	{
		const auto* files = a_quest ? a_quest->sourceFiles.array : nullptr;
		const auto* file = files && !files->empty() ? files->front() : nullptr;
		return file && !file->GetFilename().empty() ? std::string(file->GetFilename()) : std::string{};
	}
}

namespace MCMBridge
{
	Result<LiveMCM> CreateLiveMCM(
		RE::BSTSmartPointer<RE::BSScript::Object> a_script,
		std::int32_t                              a_configIndex)
	{
		if (!a_script) {
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "MCM script object is unavailable" });
		}
		auto       modName = ReadModName(a_script);
		const auto interopModName = modName;
		const auto scriptName = ReadScriptName(a_script);
		auto*      quest = ResolveQuest(a_script);
		if (!modName || modName->empty()) {
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "MCM display name is unavailable" });
		}
		if (scriptName.empty()) {
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "MCM script name is unavailable" });
		}
		if (!quest) {
			return std::unexpected(BridgeError{ BridgeErrorCode::kInvalidData, "MCM quest is unavailable" });
		}

		const auto    owner = ReadOwnerPlugin(quest);
		const auto    localFormID = quest->GetLocalFormID();
		MCMDescriptor descriptor;
		descriptor.stableID = MakeClassicModID(owner, localFormID, scriptName);
		descriptor.displayName = std::move(*modName);
		descriptor.backend = IsBasedOn(a_script, "MCM_ConfigBase") ? MCMBackendKind::kMCMHelper : MCMBackendKind::kClassicSkyUI;
		descriptor.ownerPlugin = owner;
		descriptor.questFormID = localFormID;
		descriptor.scriptName = scriptName;
		descriptor.interopID = std::format(
			"{}::{}",
			scriptName,
			interopModName.value_or(descriptor.displayName));
		descriptor.pageScopedState = IsBasedOn(a_script, "nl_mcm");
		auto adapter = std::make_shared<SkyUIHostAdapter>(a_script, *interopModName);
		return LiveMCM{ std::move(descriptor), a_configIndex, std::move(adapter), {}, {} };
	}
}
