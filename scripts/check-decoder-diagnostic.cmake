if(NOT DEFINED DECODER OR NOT DEFINED INPUT OR NOT DEFINED OUTPUT OR
   NOT DEFINED EXPECTED_STAGE OR NOT DEFINED EXPECTED_LEAF OR
   NOT DEFINED EXPECTED_PROGRESS)
    message(FATAL_ERROR "missing decoder diagnostic test argument")
endif()
if(NOT DEFINED EXPECTED_CAUSE)
    set(EXPECTED_CAUSE "cause: unsupported feature")
endif()

execute_process(
    COMMAND "${DECODER}" "${INPUT}" -o "${OUTPUT}"
    RESULT_VARIABLE decoder_result
    OUTPUT_VARIABLE decoder_stdout
    ERROR_VARIABLE decoder_stderr
)

if(decoder_result EQUAL 0)
    message(FATAL_ERROR "decoder unexpectedly succeeded")
endif()
if(EXISTS "${OUTPUT}")
    message(FATAL_ERROR "failed decode created output: ${OUTPUT}")
endif()

foreach(expected IN ITEMS
    "${EXPECTED_CAUSE}"
        "${EXPECTED_STAGE}"
        "${EXPECTED_LEAF}"
        "${EXPECTED_PROGRESS}")
    string(FIND "${decoder_stderr}" "${expected}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR
            "missing diagnostic '${expected}' in:\n${decoder_stderr}")
    endif()
endforeach()