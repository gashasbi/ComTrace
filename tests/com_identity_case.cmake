if(NOT DEFINED FIXTURE_EXE OR NOT DEFINED TEST_EXE OR NOT DEFINED COMTRACE_EXE OR NOT DEFINED COMTRACE_ARCHIVE_EXE OR NOT DEFINED MANIFEST_FILE OR NOT DEFINED TEST_DIR)
    message(FATAL_ERROR "Missing test arguments.")
endif()

file(MAKE_DIRECTORY "${TEST_DIR}")
set(ETL_FILE "${TEST_DIR}/identity_test.etl")
set(CTRA_FILE "${TEST_DIR}/identity_test.ctra")
file(REMOVE "${ETL_FILE}" "${CTRA_FILE}")

execute_process(
    COMMAND wevtutil.exe um "${MANIFEST_FILE}"
    RESULT_VARIABLE uninstall_status
    OUTPUT_QUIET
    ERROR_QUIET
)

execute_process(
    COMMAND wevtutil.exe im "${MANIFEST_FILE}"
    RESULT_VARIABLE install_status
    OUTPUT_VARIABLE install_output
    ERROR_VARIABLE install_error
)

if(NOT install_status EQUAL 0)
    message(FATAL_ERROR "Manifest installation failed: ${install_error}")
endif()

execute_process(
    COMMAND "${FIXTURE_EXE}" "${ETL_FILE}"
    RESULT_VARIABLE fixture_status
    OUTPUT_VARIABLE fixture_output
    ERROR_VARIABLE fixture_error
)

if(NOT fixture_status EQUAL 0)
    execute_process(COMMAND wevtutil.exe um "${MANIFEST_FILE}" OUTPUT_QUIET ERROR_QUIET)
    message(FATAL_ERROR "Identity fixture failed: ${fixture_error}")
endif()

execute_process(
    COMMAND "${TEST_EXE}" "${ETL_FILE}"
    RESULT_VARIABLE test_status
    OUTPUT_VARIABLE test_output
    ERROR_VARIABLE test_error
)

if(NOT test_status EQUAL 0)
    execute_process(COMMAND wevtutil.exe um "${MANIFEST_FILE}" OUTPUT_QUIET ERROR_QUIET)
    message(FATAL_ERROR "Identity extraction test failed: ${test_error}${test_output}")
endif()

execute_process(
    COMMAND "${COMTRACE_ARCHIVE_EXE}" "${ETL_FILE}" "${CTRA_FILE}"
    RESULT_VARIABLE archive_status
    OUTPUT_VARIABLE archive_output
    ERROR_VARIABLE archive_error
)

if(NOT archive_status EQUAL 0)
    execute_process(COMMAND wevtutil.exe um "${MANIFEST_FILE}" OUTPUT_QUIET ERROR_QUIET)
    message(FATAL_ERROR "ETL to CTRA conversion failed: ${archive_error}${archive_output}")
endif()

execute_process(
    COMMAND "${COMTRACE_EXE}" --compare "${ETL_FILE}" "${CTRA_FILE}"
    RESULT_VARIABLE compare_status
    OUTPUT_VARIABLE compare_output
    ERROR_VARIABLE compare_error
)

if(NOT compare_status EQUAL 0)
    execute_process(COMMAND wevtutil.exe um "${MANIFEST_FILE}" OUTPUT_QUIET ERROR_QUIET)
    message(FATAL_ERROR "ETL/CTRA comparison failed: ${compare_error}${compare_output}")
endif()

foreach(expected "Event type changes: added=0 removed=0 count_changed=0" "COM object changes: added=0 removed=0")
    string(FIND "${compare_output}" "${expected}" found)
    if(found EQUAL -1)
        execute_process(COMMAND wevtutil.exe um "${MANIFEST_FILE}" OUTPUT_QUIET ERROR_QUIET)
        message(FATAL_ERROR "Unexpected ETL/CTRA comparison output: ${expected}\n${compare_output}")
    endif()
endforeach()

execute_process(
    COMMAND "${COMTRACE_EXE}" --compare "${ETL_FILE}" "${CTRA_FILE}" --json
    RESULT_VARIABLE json_status
    OUTPUT_VARIABLE json_output
    ERROR_VARIABLE json_error
)

execute_process(
    COMMAND wevtutil.exe um "${MANIFEST_FILE}"
    RESULT_VARIABLE cleanup_status
    OUTPUT_QUIET
    ERROR_QUIET
)

if(NOT json_status EQUAL 0)
    message(FATAL_ERROR "JSON comparison failed: ${json_error}${json_output}")
endif()

foreach(expected "\"schema_version\": 1" "\"event_type_changes\": []" "\"object_changes\": []")
    string(FIND "${json_output}" "${expected}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "JSON comparison is missing expected data: ${expected}\n${json_output}")
    endif()
endforeach()

message(STATUS "COM identity extraction and ETL/CTRA comparison passed.")
