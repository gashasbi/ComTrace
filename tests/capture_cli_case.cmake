if(NOT DEFINED COMTRACE_EXE OR NOT DEFINED TEST_DIR OR NOT DEFINED CASE)
    message(FATAL_ERROR "COMTRACE_EXE, TEST_DIR, and CASE are required")
endif()

file(MAKE_DIRECTORY "${TEST_DIR}")

set(output "${TEST_DIR}/capture-case.etl")
file(REMOVE "${output}")
file(REMOVE "${TEST_DIR}/capture-case.txt")

if(CASE STREQUAL "duration_empty")
    set(duration "")
    set(expected "Invalid capture duration.")
elseif(CASE STREQUAL "duration_zero")
    set(duration "0")
    set(expected "Invalid capture duration.")
elseif(CASE STREQUAL "duration_negative")
    set(duration "-1")
    set(expected "Invalid capture duration.")
elseif(CASE STREQUAL "duration_plus")
    set(duration "+1")
    set(expected "Invalid capture duration.")
elseif(CASE STREQUAL "duration_text")
    set(duration "abc")
    set(expected "Invalid capture duration.")
elseif(CASE STREQUAL "duration_whitespace")
    set(duration " 1")
    set(expected "Invalid capture duration.")
elseif(CASE STREQUAL "duration_high")
    set(duration "3601")
    set(expected "Invalid capture duration.")
elseif(CASE STREQUAL "duration_overflow")
    set(duration "999999999999999999999999999999999999999999")
    set(expected "Invalid capture duration.")
elseif(CASE STREQUAL "empty_output")
    set(duration "1")
    set(output "")
    set(expected "An ETL output path is required.")
elseif(CASE STREQUAL "invalid_extension")
    set(duration "1")
    set(output "${TEST_DIR}/capture-case.txt")
    set(expected "Output path must use the .etl extension.")
elseif(CASE STREQUAL "existing_output")
    set(duration "1")
    file(WRITE "${output}" "existing-test-file")
    set(expected "Output file already exists:")
else()
    message(FATAL_ERROR "Unknown test case: ${CASE}")
endif()

execute_process(
    COMMAND "${COMTRACE_EXE}" --capture "${duration}" "${output}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE stdout
    ERROR_VARIABLE stderr
)

set(combined "${stdout}${stderr}")

if(NOT "${result}" STREQUAL "1")
    message(FATAL_ERROR "Expected exit code 1, received '${result}'. Output: ${combined}")
endif()

string(FIND "${combined}" "${expected}" expected_position)

if(expected_position EQUAL -1)
    message(FATAL_ERROR "Expected output was not found: '${expected}'. Output: ${combined}")
endif()
