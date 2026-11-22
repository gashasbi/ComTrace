if(NOT DEFINED FIXTURE_EXE OR NOT DEFINED TEST_EXE OR NOT DEFINED COMTRACE_EXE OR NOT DEFINED COMTRACE_ARCHIVE_EXE OR NOT DEFINED MANIFEST_FILE OR NOT DEFINED TEST_DIR)
    message(FATAL_ERROR "Missing test arguments.")
endif()

file(MAKE_DIRECTORY "${TEST_DIR}")
file(READ "${MANIFEST_FILE}" manifest_content)
string(REGEX MATCH "guid=\"\\{[0-9A-Fa-f-]+\\}\"" provider_attribute "${manifest_content}")
if(NOT provider_attribute)
    message(FATAL_ERROR "Identity fixture Provider GUID was not found.")
endif()
string(REGEX REPLACE "guid=\"([^\"]+)\"" "\\1" COMTRACE_IDENTITY_PROVIDER_GUID "${provider_attribute}")
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
    COMMAND "${COMTRACE_EXE}" --search "${ETL_FILE}" --provider "${COMTRACE_IDENTITY_PROVIDER_GUID}" --event-id 1 --max-events 10
    RESULT_VARIABLE search_status
    OUTPUT_VARIABLE search_output
    ERROR_VARIABLE search_error
)

if(NOT search_status EQUAL 0)
    execute_process(COMMAND wevtutil.exe um "${MANIFEST_FILE}" OUTPUT_QUIET ERROR_QUIET)
    message(FATAL_ERROR "Filtered ETL search failed: ${search_error}${search_output}")
endif()

string(FIND "${search_output}" "Events matching filters: 1" search_match)
if(search_match EQUAL -1)
    execute_process(COMMAND wevtutil.exe um "${MANIFEST_FILE}" OUTPUT_QUIET ERROR_QUIET)
    message(FATAL_ERROR "Provider and Event ID filters did not select the expected event: ${search_output}")
endif()

execute_process(
    COMMAND "${COMTRACE_EXE}" --search "${ETL_FILE}" --provider "${COMTRACE_IDENTITY_PROVIDER_GUID}" --event-id 5
    RESULT_VARIABLE no_match_status
    OUTPUT_VARIABLE no_match_output
    ERROR_VARIABLE no_match_error
)

if(NOT no_match_status EQUAL 0)
    execute_process(COMMAND wevtutil.exe um "${MANIFEST_FILE}" OUTPUT_QUIET ERROR_QUIET)
    message(FATAL_ERROR "Empty ETL search failed: ${no_match_error}${no_match_output}")
endif()

string(FIND "${no_match_output}" "Events matching filters: 0" no_match)
if(no_match EQUAL -1)
    execute_process(COMMAND wevtutil.exe um "${MANIFEST_FILE}" OUTPUT_QUIET ERROR_QUIET)
    message(FATAL_ERROR "Non-matching Event ID returned results: ${no_match_output}")
endif()

execute_process(
    COMMAND "${COMTRACE_EXE}" --search "${ETL_FILE}" --provider "${COMTRACE_IDENTITY_PROVIDER_GUID}" --event-id 2
    RESULT_VARIABLE clsid_alias_status
    OUTPUT_VARIABLE clsid_alias_output
    ERROR_VARIABLE clsid_alias_error
)

if(NOT clsid_alias_status EQUAL 0)
    execute_process(COMMAND wevtutil.exe um "${MANIFEST_FILE}" OUTPUT_QUIET ERROR_QUIET)
    message(FATAL_ERROR "CLSID alias event search failed: ${clsid_alias_error}${clsid_alias_output}")
endif()

string(FIND "${clsid_alias_output}" "Events matching filters: 1" clsid_alias_match)
if(clsid_alias_match EQUAL -1)
    execute_process(COMMAND wevtutil.exe um "${MANIFEST_FILE}" OUTPUT_QUIET ERROR_QUIET)
    message(FATAL_ERROR "CLSID alias event was not emitted: ${clsid_alias_output}")
endif()

execute_process(
    COMMAND "${COMTRACE_EXE}" --search "${ETL_FILE}" --provider "${COMTRACE_IDENTITY_PROVIDER_GUID}" --event-id 4
    RESULT_VARIABLE related_only_status
    OUTPUT_VARIABLE related_only_output
    ERROR_VARIABLE related_only_error
)

if(NOT related_only_status EQUAL 0)
    execute_process(COMMAND wevtutil.exe um "${MANIFEST_FILE}" OUTPUT_QUIET ERROR_QUIET)
    message(FATAL_ERROR "Related-only ActivityId event search failed: ${related_only_error}${related_only_output}")
endif()

string(FIND "${related_only_output}" "Events matching filters: 1" related_only_match)
if(related_only_match EQUAL -1)
    execute_process(COMMAND wevtutil.exe um "${MANIFEST_FILE}" OUTPUT_QUIET ERROR_QUIET)
    message(FATAL_ERROR "Related-only ActivityId event was not emitted: ${related_only_output}")
endif()

execute_process(
    COMMAND "${COMTRACE_EXE}" --search "${ETL_FILE}" --provider "${COMTRACE_IDENTITY_PROVIDER_GUID}" --event-id 3
    RESULT_VARIABLE call_trace_32_status
    OUTPUT_VARIABLE call_trace_32_output
    ERROR_VARIABLE call_trace_32_error
)

if(NOT call_trace_32_status EQUAL 0)
    execute_process(COMMAND wevtutil.exe um "${MANIFEST_FILE}" OUTPUT_QUIET ERROR_QUIET)
    message(FATAL_ERROR "32-bit CallTraceId event search failed: ${call_trace_32_error}${call_trace_32_output}")
endif()

string(FIND "${call_trace_32_output}" "Events matching filters: 1" call_trace_32_match)
if(call_trace_32_match EQUAL -1)
    execute_process(COMMAND wevtutil.exe um "${MANIFEST_FILE}" OUTPUT_QUIET ERROR_QUIET)
    message(FATAL_ERROR "32-bit CallTraceId event was not emitted: ${call_trace_32_output}")
endif()

execute_process(
    COMMAND "${COMTRACE_EXE}" --search "${ETL_FILE}" --com
    RESULT_VARIABLE com_filter_status
    OUTPUT_VARIABLE com_filter_output
    ERROR_VARIABLE com_filter_error
)

if(NOT com_filter_status EQUAL 0)
    execute_process(COMMAND wevtutil.exe um "${MANIFEST_FILE}" OUTPUT_QUIET ERROR_QUIET)
    message(FATAL_ERROR "COM search filter failed: ${com_filter_error}${com_filter_output}")
endif()

string(FIND "${com_filter_output}" "Events matching filters: 0" com_filter_match)
if(com_filter_match EQUAL -1)
    execute_process(COMMAND wevtutil.exe um "${MANIFEST_FILE}" OUTPUT_QUIET ERROR_QUIET)
    message(FATAL_ERROR "COM filter selected a non-COM fixture: ${com_filter_output}")
endif()

execute_process(
    COMMAND "${COMTRACE_EXE}" --search "${ETL_FILE}" --event-id 65536
    RESULT_VARIABLE invalid_filter_status
    OUTPUT_QUIET
    ERROR_QUIET
)

if(invalid_filter_status EQUAL 0)
    execute_process(COMMAND wevtutil.exe um "${MANIFEST_FILE}" OUTPUT_QUIET ERROR_QUIET)
    message(FATAL_ERROR "Invalid Event ID filter was accepted.")
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

foreach(expected "\"schema_version\": 1" "\"event_type_changes\": []" "\"object_changes\": []" "\"timeline_summary\":")
    string(FIND "${json_output}" "${expected}" found)
    if(found EQUAL -1)
        message(FATAL_ERROR "JSON comparison is missing expected data: ${expected}\n${json_output}")
    endif()
endforeach()

set(JSON_FILE "${TEST_DIR}/comparison.json")
file(WRITE "${JSON_FILE}" "${json_output}")
execute_process(
    COMMAND pwsh -NoProfile -Command "Get-Content -Raw -LiteralPath '${JSON_FILE}' | ConvertFrom-Json | Out-Null"
    RESULT_VARIABLE json_parse_status
    OUTPUT_VARIABLE json_parse_output
    ERROR_VARIABLE json_parse_error
)

if(NOT json_parse_status EQUAL 0)
    message(FATAL_ERROR "Comparison output is not valid JSON: ${json_parse_error}${json_parse_output}")
endif()

message(STATUS "COM identity extraction, ETL/CTRA comparison, and JSON validation passed.")
