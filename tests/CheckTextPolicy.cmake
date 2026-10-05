if(NOT DEFINED ROOT)
	get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()

file(GLOB_RECURSE POLICY_FILES
	"${ROOT}/tools/*.cpp"
	"${ROOT}/tools/*.cs"
	"${ROOT}/tools/*.csproj"
	"${ROOT}/scripts/*.psc"
	"${ROOT}/src/*.cpp"
	"${ROOT}/src/*.h"
	"${ROOT}/src/*.hpp"
	"${ROOT}/include/*.h"
	"${ROOT}/include/*.hpp"
	"${ROOT}/cmake/*.cmake"
	"${ROOT}/cmake/*.in"
	"${ROOT}/cmake/*.json"
	"${ROOT}/cmake/*.manifest"
	"${ROOT}/licenses/*.txt"
	"${ROOT}/.github/workflows/*.yml"
	"${ROOT}/config/*.ini"
	"${ROOT}/tests/*.cpp"
	"${ROOT}/tests/*.c"
	"${ROOT}/tests/*.h"
	"${ROOT}/tests/*.hpp"
	"${ROOT}/tests/*.cmake"
	"${ROOT}/tests/*.psc"
	"${ROOT}/tests/*.json"
	"${ROOT}/tests/*.ini"
)
list(APPEND POLICY_FILES
	"${ROOT}/.clang-format"
	"${ROOT}/.editorconfig"
	"${ROOT}/.gitattributes"
	"${ROOT}/.gitignore"
	"${ROOT}/CMakeLists.txt"
	"${ROOT}/CMakePresets.json"
	"${ROOT}/vcpkg.json"
	"${ROOT}/README.md"
	"${ROOT}/THIRD_PARTY_NOTICES.md"
	"${ROOT}/tests/CMakeLists.txt"
)

foreach(FILE_PATH IN LISTS POLICY_FILES)
	file(READ "${FILE_PATH}" FILE_HEX HEX)
	string(REGEX MATCHALL ".." FILE_BYTES "${FILE_HEX}")
	foreach(BYTE IN LISTS FILE_BYTES)
		if(BYTE MATCHES "^[89ABCDEFabcdef]")
			message(FATAL_ERROR "Non-ASCII byte in ${FILE_PATH}")
		endif()
		if(BYTE MATCHES "^(0[0-8bBcCeEfF]|1[0-9a-fA-F]|7[fF])$")
			message(FATAL_ERROR "Unexpected control character in ${FILE_PATH}")
		endif()
	endforeach()

	file(READ "${FILE_PATH}" FILE_TEXT)
	string(REPLACE "\r\n" "\n" FILE_TEXT "${FILE_TEXT}")
	if(FILE_TEXT MATCHES "[ \t]+\n")
		message(FATAL_ERROR "Trailing whitespace in ${FILE_PATH}")
	endif()
	if(NOT FILE_TEXT STREQUAL "" AND NOT FILE_TEXT MATCHES "\n$")
		message(FATAL_ERROR "Missing final newline in ${FILE_PATH}")
	endif()
	if(FILE_TEXT MATCHES "\r")
		message(FATAL_ERROR "Unexpected carriage return in ${FILE_PATH}")
	endif()
endforeach()
