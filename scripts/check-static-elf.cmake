foreach(required INPUT READELF NM)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

execute_process(
    COMMAND "${READELF}" -h -l -d "${INPUT}"
    RESULT_VARIABLE readelf_result
    OUTPUT_VARIABLE elf_text
    ERROR_VARIABLE readelf_error
)
if(NOT readelf_result EQUAL 0)
    message(FATAL_ERROR
        "readelf failed for ${INPUT}: ${readelf_error}")
endif()
if(NOT elf_text MATCHES "EXEC")
    message(FATAL_ERROR "${INPUT} is not a fixed-address ELF executable")
endif()
foreach(forbidden INTERP DYNAMIC NEEDED)
    if(elf_text MATCHES "${forbidden}")
        message(FATAL_ERROR
            "${INPUT} contains forbidden ELF metadata: ${forbidden}")
    endif()
endforeach()

execute_process(
    COMMAND "${NM}" -u "${INPUT}"
    RESULT_VARIABLE nm_result
    OUTPUT_VARIABLE undefined_symbols
    ERROR_VARIABLE nm_error
)
if(NOT nm_result EQUAL 0)
    message(FATAL_ERROR "nm failed for ${INPUT}: ${nm_error}")
endif()
string(STRIP "${undefined_symbols}" undefined_symbols)
if(NOT undefined_symbols STREQUAL "")
    message(FATAL_ERROR
        "${INPUT} has undefined symbols:\n${undefined_symbols}")
endif()

message(STATUS "static ELF executable has no interpreter, dynamic dependencies, or undefined symbols")