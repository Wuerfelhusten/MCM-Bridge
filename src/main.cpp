#define DLLEXPORT __declspec(dllexport)

extern "C" DLLEXPORT constinit auto SKSEPlugin_Version = [] {
	SKSE::PluginVersionData version;
	version.PluginName(Plugin::NAME);
	version.AuthorName(Plugin::AUTHOR);
	version.PluginVersion(Plugin::VERSION);
	version.UsesAddressLibrary();
	version.UsesNoStructs();
	return version;
}();

extern "C" DLLEXPORT bool SKSEAPI SKSEPlugin_Query(const SKSE::QueryInterface*, SKSE::PluginInfo* a_pluginInfo)
{
	a_pluginInfo->name = SKSEPlugin_Version.pluginName;
	a_pluginInfo->infoVersion = SKSE::PluginInfo::kVersion;
	a_pluginInfo->version = SKSEPlugin_Version.pluginVersion;
	return true;
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse, true);
	spdlog::set_pattern("[%H:%M:%S:%e] [%l] %v"s);

#ifndef NDEBUG
	spdlog::set_level(spdlog::level::trace);
	spdlog::flush_on(spdlog::level::trace);
#else
	spdlog::set_level(spdlog::level::info);
	spdlog::flush_on(spdlog::level::warn);
#endif

	SKSE::log::info("MCM Bridge loaded for game version {}", a_skse->RuntimeVersion());
	return MCMBridge::Plugin::Initialize(a_skse->RuntimeVersion().pack());
}
