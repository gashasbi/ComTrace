if(NOT DEFINED COMTRACE_EXE OR NOT DEFINED FIXTURE_EXE OR NOT DEFINED TEST_DIR)
    message(FATAL_ERROR "Missing required test parameters.")
endif()

if(NOT EXISTS "${COMTRACE_EXE}" OR NOT EXISTS "${FIXTURE_EXE}")
    message(FATAL_ERROR "Required executable not found.")
endif()

file(MAKE_DIRECTORY "${TEST_DIR}")

set(left_path "${TEST_DIR}/left.ctra")
set(right_path "${TEST_DIR}/right.ctra")
file(REMOVE "${left_path}" "${right_path}")

execute_process(
    COMMAND "${FIXTURE_EXE}" "${left_path}"
    RESULT_VARIABLE left_result
    OUTPUT_VARIABLE left_output
    ERROR_VARIABLE left_error
    TIMEOUT 30
)

if(NOT "${left_result}" STREQUAL "0")
    message(FATAL_ERROR "Unable to create left archive: ${left_output}${left_error}")
endif()

execute_process(
    COMMAND "${FIXTURE_EXE}" "${right_path}" alternate
    RESULT_VARIABLE right_result
    OUTPUT_VARIABLE right_output
    ERROR_VARIABLE right_error
    TIMEOUT 30
)

if(NOT "${right_result}" STREQUAL "0")
    message(FATAL_ERROR "Unable to create right archive: ${right_output}${right_error}")
endif()

execute_process(
    COMMAND "${COMTRACE_EXE}" --compare "${left_path}" "${right_path}" --json
    RESULT_VARIABLE compare_result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error
    TIMEOUT 30
)

if(NOT "${compare_result}" STREQUAL "0")
    message(FATAL_ERROR "JSON comparison failed: ${output}${error}")
endif()

string(JSON schema_version GET "${output}" schema_version)
if(NOT schema_version EQUAL 1)
    message(FATAL_ERROR "Unexpected JSON schema version: ${schema_version}")
endif()

string(JSON left_json_path GET "${output}" left path)
string(JSON right_json_path GET "${output}" right path)
if(NOT "${left_json_path}" STREQUAL "${left_path}" OR
   NOT "${right_json_path}" STREQUAL "${right_path}")
    message(FATAL_ERROR "JSON input paths do not match the requested archives.")
endif()

string(JSON left_events GET "${output}" left events)
string(JSON left_event_types GET "${output}" left event_types)
string(JSON right_events GET "${output}" right events)
string(JSON right_event_types GET "${output}" right event_types)
if(NOT left_events EQUAL 1 OR NOT left_event_types EQUAL 1 OR
   NOT right_events EQUAL 1 OR NOT right_event_types EQUAL 1)
    message(FATAL_ERROR "JSON trace totals are incorrect.")
endif()

string(JSON added_types GET "${output}" summary event_types_added)
string(JSON removed_types GET "${output}" summary event_types_removed)
string(JSON changed_types GET "${output}" summary event_types_count_changed)
if(NOT added_types EQUAL 1 OR NOT removed_types EQUAL 1 OR
   NOT changed_types EQUAL 0)
    message(FATAL_ERROR "JSON event type summary is incorrect.")
endif()

string(JSON event_change_count LENGTH "${output}" event_type_changes)
if(NOT event_change_count EQUAL 2)
    message(FATAL_ERROR "JSON event type change list is incomplete.")
endif()

string(JSON removed_event_id GET "${output}" event_type_changes 0 event_id)
string(JSON removed_left_count GET "${output}" event_type_changes 0 left_count)
string(JSON removed_right_count GET "${output}" event_type_changes 0 right_count)
string(JSON added_event_id GET "${output}" event_type_changes 1 event_id)
string(JSON added_left_count GET "${output}" event_type_changes 1 left_count)
string(JSON added_right_count GET "${output}" event_type_changes 1 right_count)
if(NOT removed_event_id EQUAL 91 OR NOT removed_left_count EQUAL 1 OR
   NOT removed_right_count EQUAL 0 OR NOT added_event_id EQUAL 92 OR
   NOT added_left_count EQUAL 0 OR NOT added_right_count EQUAL 1)
    message(FATAL_ERROR "JSON event type change details are incorrect.")
endif()

string(JSON object_change_count LENGTH "${output}" object_changes)
string(JSON left_timeline_count LENGTH "${output}" timeline left)
string(JSON right_timeline_count LENGTH "${output}" timeline right)
if(NOT object_change_count EQUAL 0 OR
   NOT left_timeline_count EQUAL 0 OR
   NOT right_timeline_count EQUAL 0)
    message(FATAL_ERROR "JSON object or timeline sections are incorrect.")
endif()
