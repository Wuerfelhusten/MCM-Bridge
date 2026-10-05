#include "MCMBridge/Plugin/StartupCompatibility.h"

#include "MCMBridge/Core/PexIdentity.h"
#include "ShippedScripts.h"

namespace MCMBridge
{
	std::string FindIncompatibleHostScripts()
	{
		std::string failures;
		for (const auto& script : ShippedScripts::all) {
			// Use Skyrim's resource resolver so loose files, archives and mod-manager
			// overrides are checked in the same namespace as the game's scripts.
			RE::BSResourceNiBinaryStream input(script.path);
			bool                         matches{};
			if (input.good() && input.stream && input.stream->totalSize <= 1024 * 1024) {
				std::vector<std::uint8_t> bytes(input.stream->totalSize);
				matches = !bytes.empty() && input.read(bytes.data(), static_cast<std::uint32_t>(bytes.size())) &&
				          MatchesShippedPex(bytes, script.bytes);
			}
			if (!matches) {
				SKSE::log::critical("Missing, unreadable or incompatible native host PEX: {}", script.path);
				failures += script.path;
				failures += '\n';
			}
		}
		return failures;
	}
}
