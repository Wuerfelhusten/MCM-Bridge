if(NOT EXISTS "${FORMATTER}")
	message(FATAL_ERROR "Set MCM_BRIDGE_CLANG_FORMAT to clang-format 22.1.3")
endif()
execute_process(COMMAND "${FORMATTER}" --version OUTPUT_VARIABLE VERSION RESULT_VARIABLE STATUS)
if(NOT STATUS EQUAL 0 OR NOT VERSION MATCHES "version 22\\.1\\.3([^0-9]|$)")
	message(FATAL_ERROR "Source formatting requires clang-format 22.1.3: ${VERSION}")
endif()

file(GLOB_RECURSE SOURCES
	"${ROOT}/tools/*.cpp"
	"${ROOT}/src/*.cpp" "${ROOT}/src/*.h" "${ROOT}/src/*.hpp"
	"${ROOT}/include/*.h" "${ROOT}/include/*.hpp"
	"${ROOT}/tests/*.cpp" "${ROOT}/tests/*.c"
	"${ROOT}/tests/*.h" "${ROOT}/tests/*.hpp"
)
if(MODE STREQUAL "format")
	set(OPTIONS -i)
elseif(MODE STREQUAL "format-check")
	set(OPTIONS --dry-run --Werror)
else()
	message(FATAL_ERROR "Unknown formatting mode: ${MODE}")
endif()
foreach(SOURCE IN LISTS SOURCES)
	execute_process(COMMAND "${FORMATTER}" "--style=file:${ROOT}/.clang-format" ${OPTIONS} "${SOURCE}"
		RESULT_VARIABLE STATUS)
	if(NOT STATUS EQUAL 0)
		message(FATAL_ERROR "Formatting failed: ${SOURCE}")
	endif()
endforeach()
