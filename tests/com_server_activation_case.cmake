if(NOT DEFINED COMTRACE_EXE OR NOT DEFINED CLIENT_EXE OR NOT DEFINED SERVER_DLL OR NOT DEFINED TEST_DIR)
    message(FATAL_ERROR "COM server activation test arguments are incomplete.")
endif()
if(NOT EXISTS "${COMTRACE_EXE}" OR NOT EXISTS "${CLIENT_EXE}" OR NOT EXISTS "${SERVER_DLL}")
    message(FATAL_ERROR "COM server activation test binary is missing.")
endif()
file(MAKE_DIRECTORY "${TEST_DIR}")
file(REMOVE "${TEST_DIR}/com-server-activation.etl")
execute_process(COMMAND "${CLIENT_EXE}" "${SERVER_DLL}" "${TEST_DIR}/com-server-activation.etl"
    RESULT_VARIABLE client_result OUTPUT_VARIABLE client_output ERROR_VARIABLE client_error TIMEOUT 60)
if(NOT client_result EQUAL 0)
    message(FATAL_ERROR "Real COM server activation client failed.")
endif()
if(NOT EXISTS "${TEST_DIR}/com-server-activation.etl")
    message(FATAL_ERROR "COM server client did not create an ETL file.")
endif()
execute_process(COMMAND "${COMTRACE_EXE}" --inspect "${TEST_DIR}/com-server-activation.etl" 500
    RESULT_VARIABLE inspect_result OUTPUT_VARIABLE inspect_output ERROR_VARIABLE inspect_error TIMEOUT 60)
if(NOT inspect_result EQUAL 0)
    message(FATAL_ERROR "ComTrace could not inspect the COM server ETL.")
endif()
foreach(expected
    "COM timeline summary"
    "operation=COM_CreateInstance"
    "operation=COM_GetClassObject"
    "COM object lifetime timeline summary"
    "Objects tracked: 2"
    "Objects created: 2"
    "Objects with explicit destruction: 2"
    "com_lifetime_instrumentation_id="
    "com_lifetime_clsid={7A2C9E41-6D53-4F80-9125-384B5C6D7E8F}"
    "com_lifetime_iid={8B3DA052-7E64-4091-A236-495C6D7E8F90}"
    "com_lifetime_iid={9C4EB163-8F75-41A2-B347-5A6D7E8F90A1}"
)
    string(FIND "${inspect_output}" "${expected}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "Missing COM server ETL evidence.")
    endif()
endforeach()

execute_process(COMMAND "${COMTRACE_EXE}" --compare "${TEST_DIR}/com-server-activation.etl" "${TEST_DIR}/com-server-activation.etl" --json
    RESULT_VARIABLE compare_result OUTPUT_VARIABLE compare_output ERROR_VARIABLE compare_error TIMEOUT 60)
if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR "COM lifetime JSON comparison failed: ${compare_error}")
endif()
string(FIND "${compare_output}" "\"instrumentation_id\"" instrumentation_field)
if(instrumentation_field EQUAL -1)
    message(FATAL_ERROR "COM lifetime JSON is missing the module instrumentation identifier.")
endif()

message(STATUS "Real COM activation, QueryInterface, multiple IIDs, and aggregation passed.")
