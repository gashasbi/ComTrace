if(NOT DEFINED COMTRACE_ARCHIVE_EXE OR NOT DEFINED CASE)
    message(FATAL_ERROR "Missing required test parameters.")
endif()

if(NOT EXISTS "${COMTRACE_ARCHIVE_EXE}")
    message(FATAL_ERROR "ComTraceArchive executable not found.")
endif()

if(NOT DEFINED TEST_DIR)
    message(FATAL_ERROR "Missing test directory.")
endif()

file(MAKE_DIRECTORY "${TEST_DIR}")

if(CASE STREQUAL "help")
    execute_process(
        COMMAND "${COMTRACE_ARCHIVE_EXE}" --help
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
        TIMEOUT 30
    )

    string(CONCAT combined "${output}" "${error}")

    if(NOT "${result}" STREQUAL "0")
        message(FATAL_ERROR "Help command failed: ${combined}")
    endif()

    if(NOT combined MATCHES "Usage: ComTraceArchive")
        message(FATAL_ERROR "Help output is incomplete: ${combined}")
    endif()

elseif(CASE STREQUAL "version")
    execute_process(
        COMMAND "${COMTRACE_ARCHIVE_EXE}" --version
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
        TIMEOUT 30
    )

    string(CONCAT combined "${output}" "${error}")

    if(NOT "${result}" STREQUAL "0")
        message(FATAL_ERROR "Version command failed: ${combined}")
    endif()

    if(NOT combined MATCHES "ComTraceArchive 0[.]1[.]0")
        message(FATAL_ERROR "Version output is incorrect: ${combined}")
    endif()

elseif(CASE STREQUAL "no_arguments")
    execute_process(
        COMMAND "${COMTRACE_ARCHIVE_EXE}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
        TIMEOUT 30
    )

    string(CONCAT combined "${output}" "${error}")

    if("${result}" STREQUAL "0")
        message(FATAL_ERROR "No-argument invocation unexpectedly succeeded.")
    endif()

    if(NOT combined MATCHES "Usage: ComTraceArchive")
        message(FATAL_ERROR "Usage output is missing: ${combined}")
    endif()

elseif(CASE STREQUAL "invalid_arguments")
    execute_process(
        COMMAND "${COMTRACE_ARCHIVE_EXE}" --invalid
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
        TIMEOUT 30
    )

    string(CONCAT combined "${output}" "${error}")

    if("${result}" STREQUAL "0")
        message(FATAL_ERROR "Invalid argument unexpectedly succeeded.")
    endif()

    if(NOT combined MATCHES "Usage: ComTraceArchive")
        message(FATAL_ERROR "Usage output is missing: ${combined}")
    endif()

elseif(CASE STREQUAL "missing_input")
    set(input "${TEST_DIR}/missing-input.etl")
    set(output_path "${TEST_DIR}/missing-output.ctra")

    file(REMOVE "${input}")
    file(REMOVE "${output_path}")
    file(REMOVE "${output_path}.partial")

    execute_process(
        COMMAND "${COMTRACE_ARCHIVE_EXE}" "${input}" "${output_path}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
        TIMEOUT 30
    )

    string(CONCAT combined "${output}" "${error}")

    if("${result}" STREQUAL "0")
        message(FATAL_ERROR "Missing input unexpectedly succeeded.")
    endif()

    if(EXISTS "${output_path}" OR EXISTS "${output_path}.partial")
        message(FATAL_ERROR "Missing input created an archive.")
    endif()

    if(NOT combined MATCHES "Unable to access ETL input path")
        message(FATAL_ERROR "Missing-input diagnostic is incorrect: ${combined}")
    endif()

elseif(CASE STREQUAL "inspect_missing_file")
    set(input "${TEST_DIR}/missing-archive.ctra")
    file(REMOVE "${input}")

    execute_process(
        COMMAND "${COMTRACE_ARCHIVE_EXE}" --inspect "${input}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
        TIMEOUT 30
    )

    string(CONCAT combined "${output}" "${error}")

    if("${result}" STREQUAL "0")
        message(FATAL_ERROR "Missing archive unexpectedly passed inspection.")
    endif()

    if(NOT combined MATCHES "Archive open failed")
        message(FATAL_ERROR "Missing-archive diagnostic is incorrect: ${combined}")
    endif()

elseif(CASE STREQUAL "inspect_invalid_file")
    set(input "${TEST_DIR}/invalid-archive.ctra")
    file(WRITE "${input}" "invalid")

    execute_process(
        COMMAND "${COMTRACE_ARCHIVE_EXE}" --inspect "${input}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
        TIMEOUT 30
    )

    string(CONCAT combined "${output}" "${error}")

    if("${result}" STREQUAL "0")
        message(FATAL_ERROR "Invalid archive unexpectedly passed inspection.")
    endif()

    if(NOT combined MATCHES "Archive open failed")
        message(FATAL_ERROR "Invalid-archive diagnostic is incorrect: ${combined}")
    endif()

elseif(CASE STREQUAL "recover_valid")
    if(NOT DEFINED COMTRACE_ARCHIVE_FIXTURE_EXE OR NOT EXISTS "${COMTRACE_ARCHIVE_FIXTURE_EXE}")
        message(FATAL_ERROR "Archive fixture executable is unavailable.")
    endif()

    set(input "${TEST_DIR}/recover-valid.ctra")
    file(REMOVE "${input}")

    execute_process(
        COMMAND "${COMTRACE_ARCHIVE_FIXTURE_EXE}" "${input}"
        RESULT_VARIABLE fixture_result
        OUTPUT_VARIABLE fixture_output
        ERROR_VARIABLE fixture_error
        TIMEOUT 30
    )

    if(NOT "${fixture_result}" STREQUAL "0")
        message(FATAL_ERROR "Archive fixture creation failed: ${fixture_output}${fixture_error}")
    endif()

    execute_process(
        COMMAND "${COMTRACE_ARCHIVE_EXE}" --recover "${input}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
        TIMEOUT 30
    )

    string(CONCAT combined "${output}" "${error}")

    if(NOT "${result}" STREQUAL "0")
        message(FATAL_ERROR "Archive recovery failed: ${combined}")
    endif()

    if(NOT combined MATCHES "Records recovered: 1")
        message(FATAL_ERROR "Recovered record count is incorrect: ${combined}")
    endif()

    if(NOT combined MATCHES "Stop offset: [0-9]+")
        message(FATAL_ERROR "Recovery stop offset is missing: ${combined}")
    endif()

    if(NOT combined MATCHES "Stop reason: none")
        message(FATAL_ERROR "Recovery stop reason is incorrect: ${combined}")
    endif()
else()
    message(FATAL_ERROR "Unknown archive CLI test case: ${CASE}")
endif()
