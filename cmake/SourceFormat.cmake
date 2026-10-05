find_program(MCM_BRIDGE_CLANG_FORMAT NAMES clang-format)

foreach(mode format format-check)
	add_custom_target(${mode}
		COMMAND "${CMAKE_COMMAND}"
			"-DROOT=${PROJECT_SOURCE_DIR}"
			"-DFORMATTER=${MCM_BRIDGE_CLANG_FORMAT}"
			"-DMODE=${mode}"
			-P "${CMAKE_CURRENT_LIST_DIR}/RunSourceFormat.cmake"
		VERBATIM
	)
endforeach()

if(BUILD_TESTING AND MCM_BRIDGE_CLANG_FORMAT)
	add_test(NAME SourceFormat
		COMMAND "${CMAKE_COMMAND}"
			"-DROOT=${PROJECT_SOURCE_DIR}"
			"-DFORMATTER=${MCM_BRIDGE_CLANG_FORMAT}"
			-DMODE=format-check
			-P "${CMAKE_CURRENT_LIST_DIR}/RunSourceFormat.cmake"
	)
endif()
