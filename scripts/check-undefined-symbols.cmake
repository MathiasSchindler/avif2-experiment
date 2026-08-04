if(NOT DEFINED NM OR NOT DEFINED OBJECTS OR NOT DEFINED STAMP)
    message(FATAL_ERROR "NM, OBJECTS, and STAMP are required")
endif()

string(REPLACE "|" ";" object_files "${OBJECTS}")
set(defined_symbols "")
set(undefined_symbols "")
foreach(object_file IN LISTS object_files)
    execute_process(
        COMMAND "${NM}" --format=posix "${object_file}"
        RESULT_VARIABLE nm_result
        OUTPUT_VARIABLE symbols
        ERROR_VARIABLE nm_error
    )
    if(NOT nm_result EQUAL 0)
        message(FATAL_ERROR
            "nm failed for ${object_file} with exit ${nm_result}: ${nm_error}")
    endif()

    string(REPLACE "\n" ";" symbol_lines "${symbols}")
    foreach(symbol_line IN LISTS symbol_lines)
        if(symbol_line MATCHES "^([^ ]+) ([A-Za-z?])")
            set(symbol "${CMAKE_MATCH_1}")
            set(symbol_type "${CMAKE_MATCH_2}")
            if(symbol_type MATCHES "^[Uu]$")
                list(APPEND undefined_symbols "${symbol}")
            else()
                list(APPEND defined_symbols "${symbol}")
            endif()
        endif()
    endforeach()
endforeach()

list(REMOVE_DUPLICATES defined_symbols)
list(REMOVE_DUPLICATES undefined_symbols)
foreach(defined_symbol IN LISTS defined_symbols)
    list(REMOVE_ITEM undefined_symbols "${defined_symbol}")
endforeach()
if(undefined_symbols)
    list(JOIN undefined_symbols "\n" undefined_text)
    message(FATAL_ERROR "freestanding core has undefined symbols:\n${undefined_text}")
endif()

file(TOUCH "${STAMP}")
message(STATUS "freestanding core has no undefined symbols")