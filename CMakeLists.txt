cmake_minimum_required(VERSION 3.24)

project(ComTrace VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(ComTrace
    src/main.c
    src/etw_capture.c
    src/etl_inspect.c
    src/etl_value.c
)

target_link_libraries(ComTrace PRIVATE Tdh Advapi32)

if(MSVC)
    target_compile_options(ComTrace PRIVATE /W4 /WX /utf-8)
else()
    target_compile_options(ComTrace PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

include(CTest)

if(BUILD_TESTING)
    add_executable(ComTraceEtlValueTests
        tests/test_etl_value.c
        src/etl_value.c
    )

    target_include_directories(ComTraceEtlValueTests PRIVATE src)

    if(MSVC)
        target_compile_options(ComTraceEtlValueTests PRIVATE /W4 /WX /utf-8)
    else()
        target_compile_options(ComTraceEtlValueTests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()

    add_test(NAME comtrace_etl_value COMMAND ComTraceEtlValueTests)
    add_test(NAME comtrace_smoke COMMAND ComTrace --self-test)
    add_test(NAME comtrace_version COMMAND ComTrace --version)
    add_test(NAME comtrace_provider_inventory COMMAND ComTrace --providers)
    add_test(NAME comtrace_invalid_argument COMMAND ComTrace --invalid)
    add_test(
        NAME comtrace_capture_invalid_duration
        COMMAND ComTrace --capture 0 C:/ComTrace-invalid.etl
    )
    set_tests_properties(
        comtrace_invalid_argument
        comtrace_capture_invalid_duration
        PROPERTIES WILL_FAIL TRUE
    )

    foreach(case_name invalid_limit missing_file directory_path empty_file malformed_file)
        add_test(
            NAME comtrace_inspect_${case_name}
            COMMAND "${CMAKE_COMMAND}"
                "-DCOMTRACE_EXE=$<TARGET_FILE:ComTrace>"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/inspect-tests"
                "-DCASE=${case_name}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/inspect_cli_case.cmake"
        )
    endforeach()
endif()
