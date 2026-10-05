find_package(DirectXTK CONFIG REQUIRED)
find_package(CommonLibSSE CONFIG REQUIRED)
find_package(minhook CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_path(SIMPLEINI_INCLUDE_DIR SimpleIni.h REQUIRED)

file(GLOB_RECURSE MCM_BRIDGE_CORE_SOURCES CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/Core/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/Snapshot/*.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/Write/*.cpp"
)
list(APPEND MCM_BRIDGE_CORE_SOURCES
	"${CMAKE_CURRENT_SOURCE_DIR}/src/API/HostEvents.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/API/HostData.cpp"
)

add_library(MCMBridgeCore STATIC ${MCM_BRIDGE_CORE_SOURCES})
target_compile_features(MCMBridgeCore PUBLIC cxx_std_23)
target_include_directories(MCMBridgeCore
	PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
	PRIVATE "${SIMPLEINI_INCLUDE_DIR}"
)
target_link_libraries(MCMBridgeCore PUBLIC nlohmann_json::nlohmann_json spdlog::spdlog)

add_library(MCMBridgeOperations STATIC
	"${CMAKE_CURRENT_SOURCE_DIR}/src/Papyrus/HostCallArguments.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/Papyrus/HostCallSession.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/Papyrus/ClassicHelpOperation.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/Papyrus/ClassicScanOperation.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/Papyrus/ClassicScanMetadata.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/Papyrus/ClassicWriteDialogs.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/Papyrus/ClassicWriteDispatch.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/Papyrus/ClassicWriteOperation.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/Papyrus/HostedCloseOperation.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/Papyrus/HostedPageOperation.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/src/Papyrus/HostedPageMetadata.cpp"
)
target_compile_features(MCMBridgeOperations PUBLIC cxx_std_23)
target_include_directories(MCMBridgeOperations PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(MCMBridgeOperations PUBLIC MCMBridgeCore)

file(GLOB_RECURSE MCM_BRIDGE_PLUGIN_SOURCES CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp"
)
list(FILTER MCM_BRIDGE_PLUGIN_SOURCES EXCLUDE REGEX "/src/(Core|Snapshot|Write)/")
list(FILTER MCM_BRIDGE_PLUGIN_SOURCES EXCLUDE REGEX "/src/API/HostEvents\\.cpp$")
list(FILTER MCM_BRIDGE_PLUGIN_SOURCES EXCLUDE REGEX "/src/API/HostData\\.cpp$")
list(FILTER MCM_BRIDGE_PLUGIN_SOURCES EXCLUDE REGEX "/src/Papyrus/HostCallSession\\.cpp$")
list(FILTER MCM_BRIDGE_PLUGIN_SOURCES EXCLUDE REGEX "/src/Papyrus/HostCallArguments\\.cpp$")
list(FILTER MCM_BRIDGE_PLUGIN_SOURCES EXCLUDE REGEX "/src/Papyrus/(ClassicHelpOperation|ClassicScanOperation|ClassicScanMetadata|ClassicWriteDialogs|ClassicWriteDispatch|ClassicWriteOperation|HostedCloseOperation|HostedPageOperation|HostedPageMetadata)\\.cpp$")

add_library("${PROJECT_NAME}" SHARED ${MCM_BRIDGE_PLUGIN_SOURCES})
target_link_libraries("${PROJECT_NAME}" PRIVATE shell32 ole32)
target_compile_features("${PROJECT_NAME}" PRIVATE cxx_std_23)

configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake/Plugin.h.in"
	"${CMAKE_CURRENT_BINARY_DIR}/cmake/Plugin.h"
	@ONLY
)
configure_file(
	"${CMAKE_CURRENT_SOURCE_DIR}/cmake/Version.rc.in"
	"${CMAKE_CURRENT_BINARY_DIR}/cmake/Version.rc"
	@ONLY
)
target_sources("${PROJECT_NAME}" PRIVATE
	"${CMAKE_CURRENT_BINARY_DIR}/cmake/Plugin.h"
	"${CMAKE_CURRENT_BINARY_DIR}/cmake/Version.rc"
)

target_precompile_headers("${PROJECT_NAME}" PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include/PCH.h")
target_include_directories("${PROJECT_NAME}" PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/include"
	"${CMAKE_CURRENT_SOURCE_DIR}/external/SKSEMenuFramework"
	"${CMAKE_CURRENT_BINARY_DIR}/cmake"
	"${SIMPLEINI_INCLUDE_DIR}"
)
target_link_libraries("${PROJECT_NAME}" PRIVATE
	CommonLibSSE::CommonLibSSE
	minhook::minhook
	MCMBridgeCore
	MCMBridgeOperations
)
target_compile_definitions("${PROJECT_NAME}" PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)

set(MCM_BRIDGE_DEPLOY_DIR "" CACHE PATH "Directory used for post-build plugin deployment")
add_custom_command(TARGET "${PROJECT_NAME}" POST_BUILD
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different
		"${CMAKE_CURRENT_SOURCE_DIR}/config/MCMBridge.ini"
		"$<TARGET_FILE_DIR:${PROJECT_NAME}>/MCMBridge.ini"
	VERBATIM
)

install(TARGETS "${PROJECT_NAME}" RUNTIME DESTINATION SKSE/Plugins COMPONENT Runtime)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/licenses/MinHook.txt" DESTINATION licenses/MCMBridge COMPONENT Legal)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/config/MCMBridge.ini" DESTINATION SKSE/Plugins COMPONENT Runtime)
install(FILES LICENSE THIRD_PARTY_NOTICES.md DESTINATION . COMPONENT Legal)

if(MCM_BRIDGE_DEPLOY_DIR)
	add_custom_command(TARGET "${PROJECT_NAME}" POST_BUILD
		COMMAND "${CMAKE_COMMAND}" -E make_directory "${MCM_BRIDGE_DEPLOY_DIR}"
		COMMAND "${CMAKE_COMMAND}" -E copy_if_different
			"$<TARGET_FILE:${PROJECT_NAME}>"
			"${MCM_BRIDGE_DEPLOY_DIR}/$<TARGET_FILE_NAME:${PROJECT_NAME}>"
		COMMAND "${CMAKE_COMMAND}"
			"-DSOURCE=${CMAKE_CURRENT_SOURCE_DIR}/config/MCMBridge.ini"
			"-DDESTINATION=${MCM_BRIDGE_DEPLOY_DIR}/MCMBridge.ini"
			-P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CopyDefaultSettings.cmake"
		COMMENT "Deploying ${PROJECT_NAME} to ${MCM_BRIDGE_DEPLOY_DIR}"
		VERBATIM
	)
endif()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(TARGET "${PROJECT_NAME}" PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
set_property(TARGET MCMBridgeCore PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
set_property(TARGET MCMBridgeOperations PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

if(MSVC)
	foreach(target MCMBridgeCore MCMBridgeOperations "${PROJECT_NAME}")
		target_compile_options(${target} PRIVATE
			/MP
			/permissive-
			/sdl
			/utf-8
			/W4
			/WX
			/wd4099
			/wd5054
			/Zc:preprocessor
			"$<$<CONFIG:Release>:/O2;/Ob3;/Zi>"
		)
	endforeach()
	target_link_options("${PROJECT_NAME}" PRIVATE
		"$<$<CONFIG:Debug>:/INCREMENTAL;/OPT:NOREF;/OPT:NOICF>"
		"$<$<CONFIG:Release>:/LTCG;/INCREMENTAL:NO;/OPT:REF;/OPT:ICF;/DEBUG:FULL>"
	)
endif()
