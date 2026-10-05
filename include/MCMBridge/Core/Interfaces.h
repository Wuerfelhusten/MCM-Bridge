#pragma once

#include "MCMBridge/Core/Model.h"
#include "MCMBridge/Core/Result.h"

#include <functional>
#include <memory>
#include <vector>

namespace MCMBridge
{
	struct MCMDescriptor
	{
		std::string    stableID;
		std::string    displayName;
		MCMBackendKind backend{ MCMBackendKind::kClassicSkyUI };
		std::string    ownerPlugin;
		std::uint32_t  questFormID{};
		std::string    scriptName;
		std::string    interopID;
		bool           pageScopedState{};
	};

	using Completion = std::function<void(Result<void>)>;
	using SnapshotCompletion = std::function<void(Result<MCMMod>)>;
	using WriteCompletion = std::function<void(Result<MCMValue>)>;

	class IMCMRegistryProvider
	{
	public:
		virtual ~IMCMRegistryProvider() = default;
		virtual Result<std::vector<MCMDescriptor>> Read() = 0;
		virtual bool                               IsBusy() const = 0;
	};

	class IMCMBackend
	{
	public:
		virtual ~IMCMBackend() = default;
		virtual void BuildSnapshot(const MCMDescriptor& a_descriptor, SnapshotCompletion a_completion) = 0;
		virtual void EnqueueWrite(WriteCommand a_command, WriteCompletion a_completion) = 0;
	};

	class IClassicMenuOptionResolver
	{
	public:
		virtual ~IClassicMenuOptionResolver() = default;
		virtual void                 BeginCapture(const SettingIdentity& a_identity) = 0;
		virtual void                 CancelCapture() = 0;
		virtual Result<MenuMetadata> Resolve(const SettingIdentity& a_identity) = 0;
	};

	class IWriteDispatcher
	{
	public:
		virtual ~IWriteDispatcher() = default;
		virtual void Submit(WriteCommand a_command) = 0;
	};
}
