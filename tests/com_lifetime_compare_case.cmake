if(NOT DEFINED COMTRACE_EXE OR NOT DEFINED FIXTURE_EXE OR NOT DEFINED TEST_DIR)
    message(FATAL_ERROR "Lifetime comparison test arguments are incomplete.")
endif()

if(NOT EXISTS "${COMTRACE_EXE}" OR NOT EXISTS "${FIXTURE_EXE}")
    message(FATAL_ERROR "Lifetime comparison test binary is missing.")
endif()

file(MAKE_DIRECTORY "${TEST_DIR}")
set(left_path "${TEST_DIR}/lifetime-left.etl")
set(right_path "${TEST_DIR}/lifetime-right.etl")
file(REMOVE "${TEST_DIR}/lifetime-left.etl" "${TEST_DIR}/lifetime-right.etl")

execute_process(
    COMMAND "${FIXTURE_EXE}" "${TEST_DIR}/lifetime-left.etl"
    RESULT_VARIABLE left_result
    OUTPUT_VARIABLE left_output
    ERROR_VARIABLE left_error
    TIMEOUT 30
)
if(NOT left_result EQUAL 0)
    message(FATAL_ERROR "Unable to create left lifetime trace: ${left_output}${left_error}")
endif()

execute_process(
    COMMAND "${FIXTURE_EXE}" "${TEST_DIR}/lifetime-right.etl" alternate
    RESULT_VARIABLE right_result
    OUTPUT_VARIABLE right_output
    ERROR_VARIABLE right_error
    TIMEOUT 30
)
if(NOT right_result EQUAL 0)
    message(FATAL_ERROR "Unable to create right lifetime trace: ${right_output}${right_error}")
endif()

execute_process(
    COMMAND "${COMTRACE_EXE}" --compare "${TEST_DIR}/lifetime-left.etl" "${TEST_DIR}/lifetime-right.etl" --json
    RESULT_VARIABLE compare_result
    OUTPUT_VARIABLE compare_output
    ERROR_VARIABLE compare_error
    TIMEOUT 60
)
if(NOT compare_result EQUAL 0)
    message(FATAL_ERROR "Lifetime comparison failed: ${compare_output}${compare_error}")
endif()

string(JSON changed_objects GET "${compare_output}" lifetime_summary changed)
string(JSON added_objects GET "${compare_output}" lifetime_summary added)
string(JSON removed_objects GET "${compare_output}" lifetime_summary removed)
string(JSON change_count LENGTH "${compare_output}" object_lifetime_changes)
string(JSON changed_fields GET "${compare_output}" object_lifetime_changes 0 changed_fields)
string(JSON right_interface_count LENGTH "${compare_output}" object_lifetimes right 0 interface_ids)
string(JSON right_native_count LENGTH "${compare_output}" object_lifetimes right 0 native_identities)
string(JSON right_native_object_id GET "${compare_output}" object_lifetimes right 0 native_identities 0 native_object_id)
string(JSON right_native_ipid GET "${compare_output}" object_lifetimes right 0 native_identities 0 ipid)

if(NOT changed_objects EQUAL 1 OR
   NOT added_objects EQUAL 0 OR
   NOT removed_objects EQUAL 0 OR
   NOT change_count EQUAL 1 OR
   NOT changed_fields STREQUAL "0x1a" OR
   NOT right_interface_count EQUAL 2 OR
   NOT right_native_count EQUAL 1 OR
   NOT right_native_object_id STREQUAL "0xabcdef0123456789" OR
   NOT right_native_ipid STREQUAL "{4A8D9FA4-5C7E-40B6-C439-5A7D8E9FA0B1}")
    message(FATAL_ERROR "Lifetime object differences were not reported correctly: ${compare_output}")
endif()

message(STATUS "COM lifetime comparison detected interface, lifecycle-count, and native identity changes.")
