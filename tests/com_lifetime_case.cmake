if(NOT DEFINED COMTRACE_EXE OR NOT DEFINED FIXTURE_EXE OR NOT DEFINED TEST_DIR)
    message(FATAL_ERROR "COM lifetime test arguments are incomplete.")
endif()

file(MAKE_DIRECTORY "${TEST_DIR}")
set(etl_path "${TEST_DIR}/com-lifetime.etl")
file(REMOVE "${etl_path}")

execute_process(
    COMMAND "${FIXTURE_EXE}" "${etl_path}"
    RESULT_VARIABLE fixture_result
    OUTPUT_VARIABLE fixture_output
    ERROR_VARIABLE fixture_error
)

if(NOT fixture_result EQUAL 0)
    message(FATAL_ERROR "COM lifetime fixture failed: ${fixture_output}${fixture_error}")
endif()

if(NOT EXISTS "${etl_path}")
    message(FATAL_ERROR "COM lifetime fixture did not create an ETL file.")
endif()

execute_process(
    COMMAND "${COMTRACE_EXE}" --inspect "${etl_path}" 100
    RESULT_VARIABLE inspect_result
    OUTPUT_VARIABLE inspect_output
    ERROR_VARIABLE inspect_error
)

if(NOT inspect_result EQUAL 0)
    message(FATAL_ERROR "ComTrace could not inspect the lifetime ETL: ${inspect_error}")
endif()

set(required_events
    ComObjectCreated
    ComAddRef
    ComRelease
    ComObjectDestroyed
)

set(previous_position -1)
foreach(event_name IN LISTS required_events)
    string(FIND "${inspect_output}" "com_lifetime_event=${event_name}" event_position)
    if(event_position LESS 0 OR event_position LESS_EQUAL previous_position)
        message(FATAL_ERROR "Missing or out-of-order COM lifetime event ${event_name}: ${inspect_output}")
    endif()
    set(previous_position ${event_position})
endforeach()

foreach(expected_value
    "com_lifetime_object_instance_id=1"
    "com_lifetime_clsid={183A5C71-2F49-4D83-9106-274A5B6C7D8E}"
    "com_lifetime_iid={296B7D82-3A5C-4E94-A217-385B6C7D8E9F}"
    "com_lifetime_initial_reference_count=1"
    "com_lifetime_reference_count_result=2"
    "com_lifetime_reference_count_result=1"
    "com_lifetime_reference_count_result=0"
)
    string(FIND "${inspect_output}" "${expected_value}" value_position)
    if(value_position LESS 0)
        message(FATAL_ERROR "Missing COM lifetime property ${expected_value}: ${inspect_output}")
    endif()
endforeach()

message(STATUS "COM lifetime ETW instrumentation and inspection passed.")
