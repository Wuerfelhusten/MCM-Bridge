find_package(Catch2 3 CONFIG REQUIRED)

enable_language(C)
add_library(MCMBridgeHostCCheck OBJECT HostHeaderC.c)
set_target_properties(MCMBridgeHostCCheck PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
target_include_directories(MCMBridgeHostCCheck PRIVATE "${CMAKE_SOURCE_DIR}/include")
add_library(MCMBridgeHostCppCheck OBJECT HostHeaderCpp.cpp)
target_include_directories(MCMBridgeHostCppCheck PRIVATE "${CMAKE_SOURCE_DIR}/include")
set(MCM_BRIDGE_HOST_REFERENCE "${CMAKE_SOURCE_DIR}/../MCMMemory-upstream/include/mcm/MCMBridgeHost.h" CACHE FILEPATH "Matching Memory native host header")
if(EXISTS "${MCM_BRIDGE_HOST_REFERENCE}")
	foreach(LANGUAGE IN ITEMS C Cpp)
		if(LANGUAGE STREQUAL "C")
			set(HOST_SOURCE HostHeaderC.c)
		else()
			set(HOST_SOURCE HostHeaderCpp.cpp)
		endif()
		add_library(MCMBridgeHostReference${LANGUAGE}Check OBJECT ${HOST_SOURCE})
		target_compile_definitions(MCMBridgeHostReference${LANGUAGE}Check PRIVATE MCM_HOST_CONTRACT_HEADER="${MCM_BRIDGE_HOST_REFERENCE}")
		if(MSVC)
			target_compile_options(MCMBridgeHostReference${LANGUAGE}Check PRIVATE /W4 /WX)
		endif()
	endforeach()
	set_target_properties(MCMBridgeHostReferenceCCheck PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED ON)
	add_test(NAME NativeHostReferenceContract COMMAND "${CMAKE_COMMAND}" -E compare_files
		"${CMAKE_SOURCE_DIR}/include/MCMBridge/API/MCMBridgeHost.h" "${MCM_BRIDGE_HOST_REFERENCE}")
endif()
if(MSVC)
	target_compile_options(MCMBridgeHostCCheck PRIVATE /W4 /WX)
	target_compile_options(MCMBridgeHostCppCheck PRIVATE /W4 /WX)
endif()
if(MCM_BRIDGE_SKYUI_SCRIPT_SOURCE)
	add_test(NAME NativeFacadeContract COMMAND "${CMAKE_COMMAND}"
		"-DREFERENCE=${MCM_BRIDGE_SKYUI_SCRIPT_SOURCE}/SKI_ConfigBase.psc"
		"-DFACADE=${CMAKE_SOURCE_DIR}/scripts/Source/host/SKI_ConfigBase.psc"
		-P "${CMAKE_CURRENT_SOURCE_DIR}/CheckNativeFacade.cmake")
	add_test(NAME NativeManagerContract COMMAND "${CMAKE_COMMAND}"
		"-DREFERENCE=${MCM_BRIDGE_SKYUI_SCRIPT_SOURCE}/SKI_ConfigManager.psc"
		"-DFACADE=${CMAKE_SOURCE_DIR}/scripts/Source/host/SKI_ConfigManager.psc"
		-DMINIMUM=20
		-P "${CMAKE_CURRENT_SOURCE_DIR}/CheckNativeFacade.cmake")
endif()
add_executable(MCMBridgeTests
	FrameworkRequirementsTests.cpp
	MessageWaitTests.cpp
	FacadeCallStateTests.cpp
	ExternalOperationStateTests.cpp
	HostEventsTests.cpp
	HostDataTests.cpp
	HostCallArgumentsTests.cpp
	NativeBootstrapTests.cpp
	AdapterTests.cpp
	CoreTests.cpp
	NativeMCMRegistryTests.cpp
	NativeRegistrationRequestsTests.cpp
	NativeHostSessionTests.cpp
	HelperMenuCaptureTests.cpp
	HelperMessageCallbackTests.cpp
	HelperBinaryProfileTests.cpp
	HostPageCaptureTests.cpp
	HostControlCaptureTests.cpp
	ScopedFrontendRedrawTests.cpp
	DeadlineSchedulerTests.cpp
	ActivePluginListTests.cpp
	MCMHelperTests.cpp
	MCMHelperVersionTests.cpp
	WriteTimingTests.cpp
	WritePauseTests.cpp
	BridgeSettingsTests.cpp
	MCMOrganizationTests.cpp
	SessionLifecycleTests.cpp
	HostedPageMergeTests.cpp
	ClassicPageListTests.cpp
	ViewLoadStateTests.cpp
	NavigationMergeTests.cpp
)
target_compile_features(MCMBridgeTests PRIVATE cxx_std_23)
if(TARGET native-facades)
	add_dependencies(MCMBridgeTests native-facades)
	target_sources(MCMBridgeTests PRIVATE NativeFacadePexTests.cpp)
	target_compile_definitions(MCMBridgeTests PRIVATE MCM_BRIDGE_NATIVE_SCRIPT_DIR="${CMAKE_BINARY_DIR}/native-scripts")
endif()
add_dependencies(MCMBridgeTests native-bootstrap)
target_compile_definitions(MCMBridgeTests PRIVATE MCM_BRIDGE_BOOTSTRAP_DIR="${MCM_BRIDGE_BOOTSTRAP_OUTPUT}")
set(MCM_BRIDGE_HELPER_COROUTINE_SOURCE "${CMAKE_SOURCE_DIR}/../references/MCM-Helper/src" CACHE PATH "Optional original Helper coroutine contract source")
if(EXISTS "${MCM_BRIDGE_HELPER_COROUTINE_SOURCE}/Co/Task.h")
	target_sources(MCMBridgeTests PRIVATE HelperMessageTaskTests.cpp)
	target_include_directories(MCMBridgeTests SYSTEM PRIVATE "${MCM_BRIDGE_HELPER_COROUTINE_SOURCE}")
endif()
target_link_libraries(MCMBridgeTests PRIVATE MCMBridgeCore MCMBridgeOperations Catch2::Catch2WithMain)
set_property(TARGET MCMBridgeTests PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
target_compile_definitions(MCMBridgeTests PRIVATE
	MCM_BRIDGE_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)

add_executable(MCMBridgeConfigCheck EXCLUDE_FROM_ALL ConfigCheck.cpp)
target_compile_features(MCMBridgeConfigCheck PRIVATE cxx_std_23)
target_link_libraries(MCMBridgeConfigCheck PRIVATE MCMBridgeCore)
set_property(TARGET MCMBridgeConfigCheck PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

add_executable(MCMBridgeUITests
	RendererCallbacksTests.cpp
	"${CMAKE_SOURCE_DIR}/src/Framework/RendererCallbacks.cpp"
	RichTextRendererTests.cpp
	"${CMAKE_SOURCE_DIR}/src/UI/RichTextRenderer.cpp"
)
target_compile_features(MCMBridgeUITests PRIVATE cxx_std_23)
target_compile_definitions(MCMBridgeUITests PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
target_include_directories(MCMBridgeUITests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/fakes/menu_framework")
target_link_libraries(MCMBridgeUITests PRIVATE MCMBridgeCore Catch2::Catch2WithMain)
set_property(TARGET MCMBridgeUITests PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

if(MSVC)
	# Match the plugin's full LTCG link after internal model layout changes.
	foreach(TEST_TARGET MCMBridgeTests MCMBridgeUITests MCMBridgeConfigCheck)
		target_link_options(${TEST_TARGET} PRIVATE "$<$<CONFIG:Release>:/LTCG;/INCREMENTAL:NO>")
	endforeach()
	target_compile_options(MCMBridgeTests PRIVATE /permissive- /W4 /WX /utf-8)
	target_compile_options(MCMBridgeConfigCheck PRIVATE /permissive- /W4 /WX /utf-8)
	target_compile_options(MCMBridgeUITests PRIVATE /permissive- /W4 /WX /utf-8)
endif()

include(Catch)
add_test(NAME RepositoryScripts
	COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}" "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
		-P "${CMAKE_CURRENT_SOURCE_DIR}/RepositoryScriptsTests.cmake"
)
add_test(NAME NativePackageVariants
	COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}" "-DOUTPUT=${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>"
		-P "${CMAKE_CURRENT_SOURCE_DIR}/NativePackageTests.cmake"
)
add_test(
	NAME NativeAdmissionContract
	COMMAND "${CMAKE_COMMAND}" -DROOT=${CMAKE_SOURCE_DIR}
		-P "${CMAKE_CURRENT_SOURCE_DIR}/CheckNativeAdmission.cmake"
)
add_test(
	NAME DefaultSettingsDeployment
	COMMAND "${CMAKE_COMMAND}"
		"-DROOT=${CMAKE_SOURCE_DIR}"
		"-DTEST_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}"
		-P "${CMAKE_CURRENT_SOURCE_DIR}/DefaultSettingsDeployment.cmake"
)
catch_discover_tests(MCMBridgeTests DISCOVERY_MODE PRE_TEST)
catch_discover_tests(MCMBridgeUITests DISCOVERY_MODE PRE_TEST)

add_test(
	NAME TextPolicy
	COMMAND "${CMAKE_COMMAND}"
		-DROOT=${CMAKE_SOURCE_DIR}
		-P "${CMAKE_CURRENT_SOURCE_DIR}/CheckTextPolicy.cmake"
)
