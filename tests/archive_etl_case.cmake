include("${CMAKE_CURRENT_LIST_DIR}/require_admin.cmake")
comtrace_check_admin(COMTRACE_IS_ADMIN)
if(NOT COMTRACE_IS_ADMIN)
    message("SKIP: elevated privileges required")
    return()
endif()

if(NOT DEFINED TEST_EXE OR NOT EXISTS "${TEST_EXE}")
    message(FATAL_ERROR "Archive ETL test executable is missing.")
endif()

execute_process(
    COMMAND "${TEST_EXE}"
    RESULT_VARIABLE test_status
    OUTPUT_VARIABLE test_output
    ERROR_VARIABLE test_error
)
if(NOT test_status EQUAL 0)
    message(FATAL_ERROR "Archive ETL integration test failed: ${test_output}${test_error}")
endif()

message(STATUS "Archive ETL integration test passed.")
