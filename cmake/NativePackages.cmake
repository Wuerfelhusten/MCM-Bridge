if(TARGET native-facades)
	foreach(VARIANT IN ITEMS skyui standalone)
		add_custom_target(native-package-${VARIANT}
			COMMAND "${CMAKE_COMMAND}"
				"-DROOT=${CMAKE_SOURCE_DIR}" "-DBUILD_ROOT=${CMAKE_BINARY_DIR}"
				"-DBRIDGE=$<TARGET_FILE:MCMBridge>"
				"-DPREFLIGHT=${MCM_BRIDGE_NATIVE_HOST_PREFLIGHT}"
				"-DVARIANT=${VARIANT}" -P "${CMAKE_CURRENT_LIST_DIR}/StageNativePackage.cmake"
			DEPENDS MCMBridge native-facades native-bootstrap
			VERBATIM
		)
	endforeach()
endif()
