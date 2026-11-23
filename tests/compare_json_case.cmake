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
string(JSON removed_provider_id GET "${output}" event_type_changes 0 provider_id)
string(JSON added_provider_id GET "${output}" event_type_changes 1 provider_id)
string(JSON removed_version GET "${output}" event_type_changes 0 version)
string(JSON removed_channel GET "${output}" event_type_changes 0 channel)
string(JSON removed_level GET "${output}" event_type_changes 0 level)
string(JSON removed_opcode GET "${output}" event_type_changes 0 opcode)
string(JSON removed_task GET "${output}" event_type_changes 0 task)
string(JSON removed_keyword GET "${output}" event_type_changes 0 keyword)
string(JSON added_version GET "${output}" event_type_changes 1 version)
string(JSON added_channel GET "${output}" event_type_changes 1 channel)
string(JSON added_level GET "${output}" event_type_changes 1 level)
string(JSON added_opcode GET "${output}" event_type_changes 1 opcode)
string(JSON added_task GET "${output}" event_type_changes 1 task)
string(JSON added_keyword GET "${output}" event_type_changes 1 keyword)
if(NOT removed_provider_id STREQUAL "{12345678-9ABC-DEF0-1234-56789ABCDEF0}" OR
   NOT added_provider_id STREQUAL "{12345678-9ABC-DEF0-1234-56789ABCDEF0}" OR
   NOT removed_event_id EQUAL 91 OR NOT removed_left_count EQUAL 1 OR
   NOT removed_right_count EQUAL 0 OR NOT added_event_id EQUAL 92 OR
   NOT added_left_count EQUAL 0 OR NOT added_right_count EQUAL 1 OR
   NOT removed_version EQUAL 1 OR NOT removed_channel EQUAL 2 OR
   NOT removed_level EQUAL 4 OR NOT removed_opcode EQUAL 3 OR
   NOT removed_task EQUAL 5 OR NOT removed_keyword STREQUAL "0x1122334455667788" OR
   NOT added_version EQUAL 1 OR NOT added_channel EQUAL 2 OR
   NOT added_level EQUAL 4 OR NOT added_opcode EQUAL 3 OR
   NOT added_task EQUAL 5 OR NOT added_keyword STREQUAL "0x1122334455667788")
    message(FATAL_ERROR "JSON event type change details are incorrect.")
endif()

string(JSON object_change_count LENGTH "${output}" object_changes)
string(JSON left_object_observation_count LENGTH "${output}" object_observations left)
string(JSON right_object_observation_count LENGTH "${output}" object_observations right)
string(JSON left_process_lifetime_count LENGTH "${output}" process_lifetimes left)
string(JSON right_process_lifetime_count LENGTH "${output}" process_lifetimes right)
string(JSON left_process_start_events GET "${output}" process_lifecycle_summary left start_events)
string(JSON left_process_end_events GET "${output}" process_lifecycle_summary left end_events)
string(JSON left_process_decode_failures GET "${output}" process_lifecycle_summary left decode_failures)
string(JSON right_process_start_events GET "${output}" process_lifecycle_summary right start_events)
string(JSON right_process_end_events GET "${output}" process_lifecycle_summary right end_events)
string(JSON right_process_decode_failures GET "${output}" process_lifecycle_summary right decode_failures)
string(JSON left_timeline_count LENGTH "${output}" timeline left)
string(JSON right_timeline_count LENGTH "${output}" timeline right)
string(JSON left_com_matched GET "${output}" timeline_summary left_com matched)
string(JSON left_com_call_trace_matches GET "${output}" timeline_summary left_com matched_by_call_trace_id)
string(JSON left_com_activity_matches GET "${output}" timeline_summary left_com matched_by_activity_id)
string(JSON left_com_process_thread_matches GET "${output}" timeline_summary left_com matched_by_process_thread)
string(JSON left_com_missing_call_trace GET "${output}" timeline_summary left_com missing_call_trace_ids)
string(JSON left_com_missing_activity GET "${output}" timeline_summary left_com missing_activity_ids)
string(JSON right_com_matched GET "${output}" timeline_summary right_com matched)
string(JSON right_com_call_trace_matches GET "${output}" timeline_summary right_com matched_by_call_trace_id)
string(JSON right_com_activity_matches GET "${output}" timeline_summary right_com matched_by_activity_id)
string(JSON right_com_process_thread_matches GET "${output}" timeline_summary right_com matched_by_process_thread)
string(JSON right_com_missing_call_trace GET "${output}" timeline_summary right_com missing_call_trace_ids)
string(JSON right_com_missing_activity GET "${output}" timeline_summary right_com missing_activity_ids)
string(JSON left_ole_matched GET "${output}" timeline_summary left_ole matched)
string(JSON left_ole_activity_matches GET "${output}" timeline_summary left_ole matched_by_activity_id)
string(JSON left_ole_related_activity_matches GET "${output}" timeline_summary left_ole matched_by_related_activity_id)
string(JSON left_ole_fallback_matches GET "${output}" timeline_summary left_ole matched_by_process_thread)
string(JSON left_ole_missing_activity GET "${output}" timeline_summary left_ole missing_activity_ids)
string(JSON left_ole_invalid_duration_orders GET "${output}" timeline_summary left_ole invalid_duration_orders)
string(JSON left_ole_ambiguous_stops GET "${output}" timeline_summary left_ole ambiguous_stops)
string(JSON right_ole_matched GET "${output}" timeline_summary right_ole matched)
string(JSON right_ole_activity_matches GET "${output}" timeline_summary right_ole matched_by_activity_id)
string(JSON right_ole_related_activity_matches GET "${output}" timeline_summary right_ole matched_by_related_activity_id)
string(JSON right_ole_fallback_matches GET "${output}" timeline_summary right_ole matched_by_process_thread)
string(JSON right_ole_missing_activity GET "${output}" timeline_summary right_ole missing_activity_ids)
string(JSON right_ole_invalid_duration_orders GET "${output}" timeline_summary right_ole invalid_duration_orders)
string(JSON right_ole_ambiguous_stops GET "${output}" timeline_summary right_ole ambiguous_stops)
if(NOT object_change_count EQUAL 0 OR
   NOT left_object_observation_count EQUAL 0 OR
   NOT right_object_observation_count EQUAL 0 OR
   NOT left_process_lifetime_count EQUAL 0 OR
   NOT right_process_lifetime_count EQUAL 0 OR
   NOT left_process_start_events EQUAL 0 OR
   NOT left_process_end_events EQUAL 0 OR
   NOT left_process_decode_failures EQUAL 0 OR
   NOT right_process_start_events EQUAL 0 OR
   NOT right_process_end_events EQUAL 0 OR
   NOT right_process_decode_failures EQUAL 0 OR
   NOT left_timeline_count EQUAL 0 OR
   NOT right_timeline_count EQUAL 0 OR
   NOT left_com_matched EQUAL 0 OR
   NOT left_com_call_trace_matches EQUAL 0 OR
   NOT left_com_activity_matches EQUAL 0 OR
   NOT left_com_process_thread_matches EQUAL 0 OR
   NOT left_com_missing_call_trace EQUAL 0 OR
   NOT left_com_missing_activity EQUAL 0 OR
   NOT right_com_matched EQUAL 0 OR
   NOT right_com_call_trace_matches EQUAL 0 OR
   NOT right_com_activity_matches EQUAL 0 OR
   NOT right_com_process_thread_matches EQUAL 0 OR
   NOT right_com_missing_call_trace EQUAL 0 OR
   NOT right_com_missing_activity EQUAL 0 OR
   NOT left_ole_matched EQUAL 0 OR
   NOT left_ole_activity_matches EQUAL 0 OR
   NOT left_ole_related_activity_matches EQUAL 0 OR
   NOT left_ole_fallback_matches EQUAL 0 OR
   NOT left_ole_missing_activity EQUAL 0 OR
   NOT left_ole_invalid_duration_orders EQUAL 0 OR
   NOT left_ole_ambiguous_stops EQUAL 0 OR
   NOT right_ole_matched EQUAL 0 OR
   NOT right_ole_activity_matches EQUAL 0 OR
   NOT right_ole_related_activity_matches EQUAL 0 OR
   NOT right_ole_fallback_matches EQUAL 0 OR
   NOT right_ole_missing_activity EQUAL 0 OR
   NOT right_ole_invalid_duration_orders EQUAL 0 OR
   NOT right_ole_ambiguous_stops EQUAL 0)
    message(FATAL_ERROR "JSON object or timeline sections are incorrect.")
endif()
