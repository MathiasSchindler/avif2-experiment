if(NOT DEFINED INPUT OR NOT EXISTS "${INPUT}")
    message(FATAL_ERROR "INPUT must name an existing Mach-O executable")
endif()
if(NOT DEFINED NM OR NOT EXISTS "${NM}")
    message(FATAL_ERROR "NM must name an existing symbol inspection tool")
endif()

execute_process(
    COMMAND /usr/bin/otool -L "${INPUT}"
    RESULT_VARIABLE otool_result
    OUTPUT_VARIABLE otool_output
    ERROR_VARIABLE otool_error
)
if(NOT otool_result EQUAL 0)
    message(FATAL_ERROR "otool failed: ${otool_error}")
endif()
string(REPLACE "\n" ";" otool_lines "${otool_output}")
list(LENGTH otool_lines otool_line_count)
if(otool_line_count GREATER 2)
    message(FATAL_ERROR "Mach-O executable has dylib imports:\n${otool_output}")
endif()

execute_process(
    COMMAND "${NM}" -u "${INPUT}"
    RESULT_VARIABLE nm_result
    OUTPUT_VARIABLE nm_output
    ERROR_VARIABLE nm_error
)
if(NOT nm_result EQUAL 0)
    message(FATAL_ERROR "nm failed: ${nm_error}")
endif()
string(STRIP "${nm_output}" nm_output)
if(NOT nm_output STREQUAL "")
    message(FATAL_ERROR "Mach-O executable has undefined symbols:\n${nm_output}")
endif()
