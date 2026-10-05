set(MCM_BRIDGE_PAPYRUS_COMPILER "" CACHE FILEPATH "Caprica executable for native facade scripts")
set(MCM_BRIDGE_PAPYRUS_FLAGS "" CACHE FILEPATH "TESV Papyrus flags file")
set(MCM_BRIDGE_SKSE_SCRIPT_SOURCE "" CACHE PATH "SKSE scripts directory containing vanilla and modified sources")
set(MCM_BRIDGE_SKYUI_SCRIPT_SOURCE "" CACHE PATH "SkyUI MCM source directory for compile-time compatibility imports")

option(MCM_BRIDGE_COMPILE_SCRIPTS "Recompile Papyrus sources instead of using the repository PEX files" OFF)
set(NATIVE_SCRIPT_OUTPUT "${CMAKE_BINARY_DIR}/native-scripts")
if(NOT MCM_BRIDGE_COMPILE_SCRIPTS)
	add_custom_target(native-scripts
		COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}/scripts"
			"-DBUILD_SCRIPTS=${NATIVE_SCRIPT_OUTPUT}" -DMODE=stage
			-P "${CMAKE_CURRENT_LIST_DIR}/RepositoryScripts.cmake"
		COMMAND "${CMAKE_COMMAND}" "-DINPUT=${NATIVE_SCRIPT_OUTPUT}"
			"-DOUTPUT=${CMAKE_BINARY_DIR}/cmake/ShippedScripts.h"
			-P "${CMAKE_CURRENT_LIST_DIR}/EmbedNativeScripts.cmake"
		VERBATIM
	)
	add_custom_target(native-facades DEPENDS native-scripts)
	add_dependencies(MCMBridge native-facades)
else()
	foreach(REQUIRED MCM_BRIDGE_PAPYRUS_COMPILER MCM_BRIDGE_PAPYRUS_FLAGS MCM_BRIDGE_SKSE_SCRIPT_SOURCE MCM_BRIDGE_SKYUI_SCRIPT_SOURCE)
		if(NOT ${REQUIRED})
			message(FATAL_ERROR "Script recompilation requires ${REQUIRED}")
		endif()
	endforeach()
	add_custom_command(
		OUTPUT "${NATIVE_SCRIPT_OUTPUT}/MCMBridgeNative.pex"
		COMMAND "${CMAKE_COMMAND}" -E make_directory "${NATIVE_SCRIPT_OUTPUT}"
		COMMAND "${MCM_BRIDGE_PAPYRUS_COMPILER}" --game skyrim
			--flags "${MCM_BRIDGE_PAPYRUS_FLAGS}" --all-warnings-as-errors
			--output "${NATIVE_SCRIPT_OUTPUT}" MCMBridgeNative.psc
		DEPENDS "${CMAKE_SOURCE_DIR}/scripts/Source/MCMBridgeNative.psc"
			"${MCM_BRIDGE_PAPYRUS_COMPILER}" "${MCM_BRIDGE_PAPYRUS_FLAGS}"
		WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/scripts/Source"
		VERBATIM
	)
	add_custom_target(native-scripts DEPENDS "${NATIVE_SCRIPT_OUTPUT}/MCMBridgeNative.pex")
	if(MCM_BRIDGE_SKSE_SCRIPT_SOURCE AND MCM_BRIDGE_SKYUI_SCRIPT_SOURCE)
		set(NATIVE_IMPORTS "${CMAKE_BINARY_DIR}/native-imports")
		add_custom_target(native-facades
			COMMAND "${CMAKE_COMMAND}" "-DSKSE_SCRIPTS=${MCM_BRIDGE_SKSE_SCRIPT_SOURCE}"
				"-DOUTPUT=${NATIVE_IMPORTS}" -P "${CMAKE_CURRENT_LIST_DIR}/PreparePapyrusImports.cmake"
			COMMAND "${MCM_BRIDGE_PAPYRUS_COMPILER}" --game skyrim
				--allow-unknown-events true --skyrim-allow-unknown-events-on-non-native-class true
				--disable-warning 7000 --disable-warning 4007 --disable-warning 4006
				--flags "${MCM_BRIDGE_PAPYRUS_FLAGS}" --all-warnings-as-errors
				--import "${CMAKE_SOURCE_DIR}/scripts/Source;${NATIVE_IMPORTS}"
				--output "${NATIVE_SCRIPT_OUTPUT}" SKI_ConfigBase.psc
			COMMAND "${MCM_BRIDGE_PAPYRUS_COMPILER}" --game skyrim
				--allow-unknown-events true --skyrim-allow-unknown-events-on-non-native-class true
				--disable-warning 7000 --disable-warning 4007 --disable-warning 4006
				--flags "${MCM_BRIDGE_PAPYRUS_FLAGS}" --all-warnings-as-errors
				--import "${CMAKE_SOURCE_DIR}/scripts/Source;${NATIVE_IMPORTS}"
				--output "${NATIVE_SCRIPT_OUTPUT}" SKI_ConfigManager.psc
			COMMAND "${MCM_BRIDGE_PAPYRUS_COMPILER}" --game skyrim
				--flags "${MCM_BRIDGE_PAPYRUS_FLAGS}" --all-warnings-as-errors
				--import "${CMAKE_SOURCE_DIR}/scripts/Source;${NATIVE_IMPORTS}"
				--output "${NATIVE_SCRIPT_OUTPUT}" MCMBridgeRegistry.psc
			WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/scripts/Source/host"
			VERBATIM
		)
		add_dependencies(native-facades native-scripts)
		add_custom_command(TARGET native-facades POST_BUILD
			COMMAND "${CMAKE_COMMAND}" "-DINPUT=${NATIVE_SCRIPT_OUTPUT}"
				"-DOUTPUT=${CMAKE_BINARY_DIR}/cmake/ShippedScripts.h"
				-P "${CMAKE_CURRENT_LIST_DIR}/EmbedNativeScripts.cmake"
			VERBATIM
		)
		add_dependencies(MCMBridge native-facades)
		foreach(BOOTSTRAP_SCRIPT IN ITEMS SKI_QuestBase SKI_PlayerLoadGameAlias)
			add_custom_command(TARGET native-facades POST_BUILD
				COMMAND "${MCM_BRIDGE_PAPYRUS_COMPILER}" --game skyrim
					--allow-unknown-events true --skyrim-allow-unknown-events-on-non-native-class true
					--disable-warning 7000
					--flags "${MCM_BRIDGE_PAPYRUS_FLAGS}" --all-warnings-as-errors
					--import "${CMAKE_SOURCE_DIR}/scripts/Source;${NATIVE_IMPORTS}"
					--output "${NATIVE_SCRIPT_OUTPUT}" "${BOOTSTRAP_SCRIPT}.psc"
				WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/scripts/Source/host"
				VERBATIM
			)
		endforeach()
	endif()
	# Only this explicit target updates repository binaries after recompilation.
	add_custom_target(refresh-precompiled-scripts
		COMMAND "${CMAKE_COMMAND}" "-DROOT=${CMAKE_SOURCE_DIR}/scripts"
			"-DBUILD_SCRIPTS=${NATIVE_SCRIPT_OUTPUT}" -DMODE=refresh
			-P "${CMAKE_CURRENT_LIST_DIR}/RepositoryScripts.cmake"
		DEPENDS native-facades
		VERBATIM
	)
endif()

if(MCM_BRIDGE_SKYUI_SCRIPT_SOURCE)
	foreach(BOOTSTRAP_SCRIPT IN ITEMS SKI_QuestBase SKI_PlayerLoadGameAlias)
		add_test(NAME "${BOOTSTRAP_SCRIPT}Contract"
			COMMAND "${CMAKE_COMMAND}"
				"-DREFERENCE=${MCM_BRIDGE_SKYUI_SCRIPT_SOURCE}/${BOOTSTRAP_SCRIPT}.psc"
				"-DFACADE=${CMAKE_SOURCE_DIR}/scripts/Source/host/${BOOTSTRAP_SCRIPT}.psc"
				-DMINIMUM=1 -P "${CMAKE_SOURCE_DIR}/tests/CheckNativeFacade.cmake"
		)
	endforeach()
endif()

# The standard install is the SkyUI add-on. Standalone staging adds its own
# quest/base scripts together with the generated compatibility ESP and SEQ.
foreach(SCRIPT MCMBridgeNative MCMBridgeRegistry SKI_ConfigBase SKI_ConfigManager)
	install(FILES "${NATIVE_SCRIPT_OUTPUT}/${SCRIPT}.pex" DESTINATION Scripts COMPONENT Runtime)
endforeach()
