add_executable(MCMBridgeBootstrap tools/BootstrapPlugin.cpp)
target_compile_features(MCMBridgeBootstrap PRIVATE cxx_std_23)
if(MSVC)
	target_compile_options(MCMBridgeBootstrap PRIVATE /W4 /WX /permissive-)
endif()
set(MCM_BRIDGE_BOOTSTRAP_OUTPUT "${CMAKE_BINARY_DIR}/native-bootstrap")
add_custom_command(
	OUTPUT "${MCM_BRIDGE_BOOTSTRAP_OUTPUT}/SkyUI_SE.esp" "${MCM_BRIDGE_BOOTSTRAP_OUTPUT}/Seq/SkyUI_SE.seq"
	COMMAND MCMBridgeBootstrap "${MCM_BRIDGE_BOOTSTRAP_OUTPUT}"
	DEPENDS MCMBridgeBootstrap
	VERBATIM
)
add_custom_target(native-bootstrap
	DEPENDS "${MCM_BRIDGE_BOOTSTRAP_OUTPUT}/SkyUI_SE.esp" "${MCM_BRIDGE_BOOTSTRAP_OUTPUT}/Seq/SkyUI_SE.seq"
)
