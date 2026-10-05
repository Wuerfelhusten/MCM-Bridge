#include "MCMBridge/Plugin/StartupCompatibility.h"

#include "MCMBridge/Core/ActivePluginList.h"

#include <ShlObj.h>
#include <filesystem>
#include <fstream>
#include <memory>

namespace MCMBridge
{
	Result<bool> IsRecorderEnabledAtStartup(std::uint32_t a_packedRuntime)
	try {
		const auto folder = PluginListFolder(a_packedRuntime);
		if (!folder)
			return std::unexpected(folder.error());
		PWSTR      localAppData{};
		const auto result = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &localAppData);
		if (FAILED(result)) {
			if (localAppData)
				CoTaskMemFree(localAppData);
			return std::unexpected(BridgeError{ BridgeErrorCode::kUnavailable, "Could not resolve Local AppData" });
		}
		const std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)> buffer(localAppData, CoTaskMemFree);
		const auto                                               base = std::filesystem::path(buffer.get());
		const auto                                               path = base / *folder / "plugins.txt";
		std::ifstream                                            input(path, std::ios::binary);
		if (!input)
			return std::unexpected(BridgeError{ BridgeErrorCode::kIoError, "Could not open plugins.txt" });
		return IsPluginActive(input, "McmRecorder.esp");
	} catch (const std::exception&) {
		return std::unexpected(BridgeError{ BridgeErrorCode::kIoError, "Could not inspect plugins.txt" });
	}
}
