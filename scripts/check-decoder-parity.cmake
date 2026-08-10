foreach(required DECODER INPUT OUTPUT EXPECTED_SHA256)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

get_filename_component(output_directory "${OUTPUT}" DIRECTORY)
file(MAKE_DIRECTORY "${output_directory}")
file(REMOVE "${OUTPUT}")
execute_process(
    COMMAND "${DECODER}" "${INPUT}" -o "${OUTPUT}"
    RESULT_VARIABLE decode_result
    OUTPUT_VARIABLE decode_stdout
    ERROR_VARIABLE decode_stderr
)
if(NOT decode_result EQUAL 0)
    message(FATAL_ERROR
        "decoder exited ${decode_result}: ${decode_stderr}")
endif()

file(SHA256 "${OUTPUT}" actual_sha256)
file(REMOVE "${OUTPUT}")
if(NOT actual_sha256 STREQUAL EXPECTED_SHA256)
    message(FATAL_ERROR
        "expected ${EXPECTED_SHA256}, got ${actual_sha256}")
endif()

message(STATUS "decoder output matches ${EXPECTED_SHA256}")