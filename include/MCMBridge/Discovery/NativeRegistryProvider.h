#pragma once

#include "MCMBridge/Discovery/LiveMCM.h"
#include "MCMBridge/Papyrus/NativeRegistryMirror.h"

namespace MCMBridge
{
	struct NativeMenuRegistration
	{
		RE::BSTSmartPointer<RE::BSScript::Object> object;
		std::string                               name;
	};
	// Game-task owned. The neutral registry owns identities; this adapter retains
	// VM objects and installs complete compatibility mirrors at registration boundaries.
	class NativeRegistryProvider final : public ILiveMCMRegistryProvider
	{
	public:
		Result<bool>                       Bootstrap(RE::BSTSmartPointer<RE::BSScript::Object> a_manager, std::uint64_t a_session);
		Result<std::int32_t>               Register(RE::BSTSmartPointer<RE::BSScript::Object> a_menu, std::string a_name);
		Result<std::vector<std::int32_t>>  RegisterBatch(std::span<const NativeMenuRegistration> a_menus);
		Result<std::int32_t>               Unregister(const RE::BSTSmartPointer<RE::BSScript::Object>& a_menu);
		void                               Reset();
		bool                               SetActive(const RE::BSTSmartPointer<RE::BSScript::Object>& a_menu);
		void                               ClearActive(const RE::BSTSmartPointer<RE::BSScript::Object>& a_menu);
		Result<bool>                       Clear();
		Result<std::vector<MCMDescriptor>> Read() override;
		Result<std::vector<LiveMCM>>       ReadLive() override;
		bool                               IsBusy() const override { return false; }
		bool                               IsAvailable() const override { return manager && session != 0; }
		std::string_view                   Name() const override { return "Native MCM host"; }
		std::uint64_t                      Session() const { return session; }
		std::uint64_t                      Revision() const { return registry.Revision(); }
		Result<std::size_t>                Count() const;
		Result<std::string>                ResolveIdentity(const RE::BSTSmartPointer<RE::BSScript::Object>& a_menu) const;
		bool                               Owns(const RE::BSTSmartPointer<RE::BSScript::Object>& a_manager) const { return IsAvailable() && manager == a_manager; }

	private:
		bool                                      Install(const NativeRegistryView& a_view, NativeRegistryMirror a_mirror);
		bool                                      MirrorMatches(std::int32_t a_slot, const RE::BSTSmartPointer<RE::BSScript::Object>& a_menu, std::string_view a_name) const;
		NativeRegistryMirror                      installedMirror;
		std::int32_t                              installedCount{};
		NativeMCMRegistry                         registry;
		std::uint64_t                             session{};
		std::uint64_t                             registrySession{};
		std::uint64_t                             nextInstance{};
		RE::BSTSmartPointer<RE::BSScript::Object> manager;
		std::vector<NativeObjectBinding>          bindings;
	};
}
