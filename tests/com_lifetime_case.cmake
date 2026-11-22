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

foreach(expected_text
    "COM object lifetime timeline summary"
    "Objects tracked: 1"
    "Objects created: 1"
    "Objects with explicit destruction: 1"
    "instrumented_objects=1"
    "process_owns_lifetime_object"
)
    string(FIND "${inspect_output}" "${expected_text}" text_position)
    if(text_position LESS 0)
        message(FATAL_ERROR "Missing COM lifetime analysis output ${expected_text}: ${inspect_output}")
    endif()
endforeach()

execute_process(
    COMMAND "${COMTRACE_EXE}" --search "${etl_path}" --lifetime-id 1 --max-events 100
    RESULT_VARIABLE lifetime_search_result
    OUTPUT_VARIABLE lifetime_search_output
    ERROR_VARIABLE lifetime_search_error
)
if(NOT lifetime_search_result EQUAL 0)
    message(FATAL_ERROR "Lifetime object search failed: ${lifetime_search_error}")
endif()
string(FIND "${lifetime_search_output}" "com_lifetime_event=ComObjectCreated" lifetime_search_position)
if(lifetime_search_position LESS 0)
    message(FATAL_ERROR "Lifetime object ID filter did not return its creation event.")
endif()

execute_process(
    COMMAND "${COMTRACE_EXE}" --search "${etl_path}" --clsid "{183A5C71-2F49-4D83-9106-274A5B6C7D8E}" --max-events 100
    RESULT_VARIABLE clsid_search_result
    OUTPUT_VARIABLE clsid_search_output
    ERROR_VARIABLE clsid_search_error
)
if(NOT clsid_search_result EQUAL 0)
    message(FATAL_ERROR "CLSID search failed: ${clsid_search_error}")
endif()
string(FIND "${clsid_search_output}" "com_lifetime_event=ComObjectCreated" clsid_search_position)
if(clsid_search_position LESS 0)
    message(FATAL_ERROR "CLSID filter did not return the matching COM creation event.")
endif()

execute_process(
    COMMAND "${COMTRACE_EXE}" --search "${etl_path}" --ole --max-events 100
    RESULT_VARIABLE ole_search_result
    OUTPUT_VARIABLE ole_search_output
    ERROR_VARIABLE ole_search_error
)
if(NOT ole_search_result EQUAL 0)
    message(FATAL_ERROR "OLE search filter failed: ${ole_search_error}")
endif()
string(FIND "${ole_search_output}" "Events matching filters: 0" ole_search_position)
if(ole_search_position LESS 0)
    message(FATAL_ERROR "OLE search filter included non-OLE lifetime events.")
endif()

execute_process(
    COMMAND "${COMTRACE_EXE}" --search "${etl_path}" --iid "{296B7D82-3A5C-4E94-A217-385B6C7D8E9F}" --max-events 100
    RESULT_VARIABLE iid_search_result
    OUTPUT_VARIABLE iid_search_output
    ERROR_VARIABLE iid_search_error
)
if(NOT iid_search_result EQUAL 0)
    message(FATAL_ERROR "IID search failed: ${iid_search_error}")
endif()
string(FIND "${iid_search_output}" "com_lifetime_event=ComObjectCreated" iid_search_position)
if(iid_search_position LESS 0)
    message(FATAL_ERROR "IID search did not return the matching COM lifetime event.")
endif()

execute_process(
    COMMAND "${COMTRACE_EXE}" --compare "${etl_path}" "${etl_path}" --json
    RESULT_VARIABLE compare_result
    OUTPUT_VARIABLE compare_output
    ERROR_VARIABLE compare_error
)
if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR "Lifetime trace comparison failed: ${compare_error}")
endif()

foreach(expected "lifetime_summary" "object_lifetimes" "object_lifetime_changes" "object_instance_id" "interface_ids")
    string(FIND "${compare_output}" "${expected}" compare_position)
    if(compare_position LESS 0)
        message(FATAL_ERROR "Lifetime comparison JSON is missing ${expected}.")
    endif()
endforeach()

message(STATUS "COM lifetime inspection, identity search, and comparison JSON passed.")
