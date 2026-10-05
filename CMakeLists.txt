cmake_minimum_required(VERSION 3.28)
project(MCMBridge VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(MCM_BRIDGE_AUTHOR "MCM Bridge Contributors")

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(SKSEPlugin)
include(NativeScripts)
include(NativeBootstrap)

option(MCM_BRIDGE_NATIVE_HOST_PREFLIGHT "Probe temporary VM array storage without activating the native host" OFF)
if(MCM_BRIDGE_NATIVE_HOST_PREFLIGHT)
	set_property(SOURCE src/Plugin/Plugin.cpp APPEND PROPERTY COMPILE_DEFINITIONS MCM_BRIDGE_NATIVE_HOST_PREFLIGHT)
	set_property(SOURCE src/Plugin/BridgeControllerScan.cpp APPEND PROPERTY COMPILE_DEFINITIONS MCM_BRIDGE_NATIVE_HOST_PREFLIGHT)
endif()
include(NativePackages)

if(MSVC_VERSION GREATER_EQUAL 1936 AND MSVC_IDE) # 17.6+
	file(CONFIGURE OUTPUT "${CMAKE_BINARY_DIR}/Directory.Build.props" CONTENT [==[
	<Project>
	  <ItemDefinitionGroup>
		<ClCompile>
		  <BuildStlModules>false</BuildStlModules>
		</ClCompile>
	  </ItemDefinitionGroup>
	</Project>
	]==] @ONLY)
endif()

include(CTest)
include(SourceFormat)
if(BUILD_TESTING)
	add_subdirectory(tests)
endif()

set(CPACK_GENERATOR ZIP)
set(CPACK_PACKAGE_NAME MCMBridge)
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "MCMBridge-${PROJECT_VERSION}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}/packages")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
include(CPack)
