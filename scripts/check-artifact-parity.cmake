foreach(required DECODER ARTIFACT_DIR MANIFEST OUTPUT_DIR EXPECTED_COUNT)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

file(STRINGS "${MANIFEST}" entries)
list(LENGTH entries entry_count)
if(NOT entry_count EQUAL EXPECTED_COUNT)
    message(FATAL_ERROR
        "expected ${EXPECTED_COUNT} parity entries, found ${entry_count}")
endif()

file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")
set(failures "")

foreach(entry IN LISTS entries)
    if(NOT entry MATCHES "^([^|]+)[|]([0-9a-f]+)$")
        list(APPEND failures "invalid manifest entry: ${entry}")
        continue()
    endif()

    set(name "${CMAKE_MATCH_1}")
    set(expected_sha256 "${CMAKE_MATCH_2}")
    set(input "${ARTIFACT_DIR}/${name}.obu")
    set(output "${OUTPUT_DIR}/${name}.i420")
    execute_process(
        COMMAND "${DECODER}" "${input}" -o "${output}"
        RESULT_VARIABLE decode_result
        OUTPUT_VARIABLE decode_stdout
        ERROR_VARIABLE decode_stderr
    )
    if(NOT decode_result EQUAL 0)
        list(APPEND failures
            "${name}: decoder exited ${decode_result}: ${decode_stderr}")
        continue()
    endif()

    file(SHA256 "${output}" actual_sha256)
    if(NOT actual_sha256 STREQUAL expected_sha256)
        list(APPEND failures
            "${name}: expected ${expected_sha256}, got ${actual_sha256}")
    endif()
endforeach()

file(REMOVE_RECURSE "${OUTPUT_DIR}")
if(failures)
    list(JOIN failures "\n" failure_text)
    message(FATAL_ERROR "artifact parity failed:\n${failure_text}")
endif()

message(STATUS "artifact parity: ${entry_count}/${EXPECTED_COUNT} exact")