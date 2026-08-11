foreach(required DECODER FIXTURE_DIRECTORY OUTPUT_DIRECTORY)
    if(NOT DEFINED ${required})
        message(FATAL_ERROR "${required} is required")
    endif()
endforeach()

file(MAKE_DIRECTORY "${OUTPUT_DIRECTORY}")

function(check_bmp fixture name expected_sha256)
    set(output "${OUTPUT_DIRECTORY}/${name}.bmp")
    file(REMOVE "${output}")
    execute_process(
        COMMAND "${DECODER}" "${FIXTURE_DIRECTORY}/${fixture}" -o "${output}"
        RESULT_VARIABLE result
        ERROR_VARIABLE standard_error
    )
    if(NOT result EQUAL 0)
        message(FATAL_ERROR
            "${name} BMP decode exited ${result}: ${standard_error}")
    endif()
    file(SHA256 "${output}" actual_sha256)
    file(REMOVE "${output}")
    if(NOT actual_sha256 STREQUAL expected_sha256)
        message(FATAL_ERROR
            "${name} BMP expected ${expected_sha256}, got ${actual_sha256}")
    endif()
endfunction()

function(check_rejected fixture name extension)
    set(output "${OUTPUT_DIRECTORY}/${name}.${extension}")
    file(REMOVE "${output}")
    execute_process(
        COMMAND "${DECODER}" "${FIXTURE_DIRECTORY}/${fixture}" -o "${output}"
        RESULT_VARIABLE result
    )
    if(NOT result EQUAL 2)
        message(FATAL_ERROR
            "${name}.${extension} rejection exited ${result}, expected 2")
    endif()
    if(EXISTS "${output}")
        message(FATAL_ERROR "rejected output was created: ${output}")
    endif()
endfunction()

check_bmp(
    monochrome-odd.obu i400
    2cf48f28100237d4ac9656e17f3cdf2c38c8bb5dd2b3c8af999a159cba513a8b)
check_bmp(
    ordinary-160x120.obu i420
    66ebb65fe21ec8ec8fbfe46fc009b0cf395dba2a22c8f6f91df45eaaa867c20f)
check_bmp(
    i444-odd.obu i444
    782695dcf08bce63864ea9187d3e0fd472a9681547671805c299eb474b2f6469)

check_rejected(ordinary-160x120.obu generic yuv)
check_rejected(ordinary-160x120.obu mismatched i444)

message(STATUS "BMP hashes and strict raw-output suffix policy match")