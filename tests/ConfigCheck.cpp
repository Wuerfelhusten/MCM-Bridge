#include "MCMBridge/Core/MCMHelperParser.h"

#include <exception>
#include <format>
#include <iostream>

int main(int argc, char* argv[])
{
	if (argc < 2) {
		std::cerr << "Usage: MCMBridgeConfigCheck <config.json> [config.json ...]\n";
		return 2;
	}
	bool failed{};
	for (int index = 1; index < argc; ++index) {
		const std::filesystem::path path(argv[index]);
		try {
			auto result = MCMBridge::MCMHelperParser{}.Parse({ .config = path,
				.defaults = path.parent_path() / "settings.ini" });
			if (!result) {
				std::cerr << std::format("FAIL {}: {}\n", path.string(), result.error().message);
				failed = true;
				continue;
			}
			std::size_t controls{};
			for (const auto& page : result->pages) {
				controls += page.controls.size();
			}
			std::cout << std::format("PASS {}: minimum={} pages={} controls={}\n",
				path.string(), result->minimumMCMHelperVersion, result->pages.size(), controls);
		} catch (const std::exception& error) {
			std::cerr << std::format("FAIL {}: {}\n", path.string(), error.what());
			failed = true;
		}
	}
	return failed ? 1 : 0;
}
