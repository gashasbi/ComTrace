include("${CMAKE_CURRENT_LIST_DIR}/require_admin.cmake")
comtrace_check_admin(COMTRACE_IS_ADMIN)
if(NOT COMTRACE_IS_ADMIN)
    message("SKIP: elevated privileges required")
    return()
endif()

if(NOT DEFINED COMTRACE_EXE OR NOT DEFINED FIXTURE_EXE OR NOT DEFINED TEST_DIR)
    message(FATAL_ERROR "DDE instrumentation test arguments are incomplete.")
endif()

file(MAKE_DIRECTORY "${TEST_DIR}")
set(etl_path "${TEST_DIR}/dde-instrumentation.etl")
file(REMOVE "${etl_path}")

execute_process(
    COMMAND "${FIXTURE_EXE}" "${etl_path}"
    RESULT_VARIABLE fixture_result
    OUTPUT_VARIABLE fixture_output
    ERROR_VARIABLE fixture_error
)

if(NOT fixture_result EQUAL 0)
    message(FATAL_ERROR "DDE instrumentation fixture failed: ${fixture_output}${fixture_error}")
endif()

if(NOT EXISTS "${etl_path}")
    message(FATAL_ERROR "DDE instrumentation fixture did not create an ETL file.")
endif()

execute_process(
    COMMAND "${COMTRACE_EXE}" --inspect "${etl_path}" 100
    RESULT_VARIABLE inspect_result
    OUTPUT_VARIABLE inspect_output
    ERROR_VARIABLE inspect_error
)

if(NOT inspect_result EQUAL 0)
    message(FATAL_ERROR "ComTrace could not inspect the DDE ETL: ${inspect_error}")
endif()

foreach(expected_text
    "DDE timeline summary"
    "Conversation records: 2"
    "Transactions started: 1"
    "Transactions completed: 1"
    "Server/client callbacks: 1"
    "Correlated client/server exchanges: 1"
    "Unmatched completions: 0"
    "Unmatched conversation closes: 0"
    "Ambiguous conversation closes: 0"
    "Invalid duration timestamp orders: 0"
    "Transaction metadata mismatches: 0"
    "Missing transaction correlation IDs: 0"
    "Ambiguous cross-process correlations: 0"
    "dde_callbacks=1"
    "dde_exchanges=1"
    "dde_callback_links=1"
    "dde_instance=1"
    "dde_instance=2"
    "role=client"
    "item=Item"
    "service=ComTraceService topic=ComTraceTopic"
)
    string(FIND "${inspect_output}" "${expected_text}" text_position)
    if(text_position LESS 0)
        message(FATAL_ERROR "Missing DDE inspection output ${expected_text}: ${inspect_output}")
    endif()
endforeach()
