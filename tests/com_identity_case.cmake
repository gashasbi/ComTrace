if(NOT DEFINED FIXTURE_EXE OR NOT DEFINED TEST_EXE OR NOT DEFINED MANIFEST_FILE OR NOT DEFINED TEST_DIR)
    message(FATAL_ERROR "Missing test arguments.")
endif()

file(MAKE_DIRECTORY "${TEST_DIR}")
set(ETL_FILE "${TEST_DIR}/identity_test.etl")
file(REMOVE "${ETL_FILE}")

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

execute_process(
    COMMAND wevtutil.exe um "${MANIFEST_FILE}"
    RESULT_VARIABLE cleanup_status
    OUTPUT_QUIET
    ERROR_QUIET
)

if(NOT test_status EQUAL 0)
    message(FATAL_ERROR "Identity extraction test failed: ${test_error}${test_output}")
endif()

message(STATUS "COM identity GUID properties were extracted correctly.")
