cmake_minimum_required(VERSION 3.21)

cmake_policy(SET CMP0091 NEW)

project(ComTrace VERSION 0.1.0 LANGUAGES C)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(ComTrace
    src/main.c
    src/etw_capture.c
    src/etl_inspect.c
    src/etl_value.c
    src/com_event.c
    src/ole_event.c
)

target_link_libraries(ComTrace PRIVATE Tdh Advapi32)

if(MINGW)
    target_link_options(ComTrace PRIVATE -municode)
endif()

if(MSVC)
    target_compile_options(ComTrace PRIVATE /W4 /WX /wd5105 /utf-8)
else()
    target_compile_options(ComTrace PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

include(CTest)

if(BUILD_TESTING)
    add_executable(ComTraceComEventTests
        tests/test_com_event.c
        src/com_event.c
    )

    target_include_directories(ComTraceComEventTests PRIVATE src)

    if(MSVC)
        target_compile_options(ComTraceComEventTests PRIVATE /W4 /WX /wd5105 /utf-8)
    else()
        target_compile_options(ComTraceComEventTests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()

    add_test(NAME comtrace_com_event COMMAND ComTraceComEventTests)

    add_executable(ComTraceOleEventTests
        tests/test_ole_event.c
        src/ole_event.c
    )
    target_include_directories(ComTraceOleEventTests PRIVATE src)
    target_compile_definitions(ComTraceOleEventTests PRIVATE WIN32_LEAN_AND_MEAN)

    if(MSVC)
        target_compile_options(ComTraceOleEventTests PRIVATE /W4 /WX /wd5105 /utf-8)
    else()
        target_compile_options(ComTraceOleEventTests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()

    add_test(NAME comtrace_ole_event COMMAND ComTraceOleEventTests)
    add_executable(ComTraceEtlValueTests
        tests/test_etl_value.c
        src/etl_value.c
    )

    target_include_directories(ComTraceEtlValueTests PRIVATE src)

    if(MSVC)
        target_compile_options(ComTraceEtlValueTests PRIVATE /W4 /WX /wd5105 /utf-8)
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

    foreach(case_name duration_empty duration_zero duration_negative duration_plus duration_text duration_whitespace duration_high duration_overflow duration_minimum duration_maximum empty_output invalid_extension existing_output missing_directory)
        add_test(
            NAME comtrace_capture_${case_name}
            COMMAND "${CMAKE_COMMAND}"
                "-DCOMTRACE_EXE=$<TARGET_FILE:ComTrace>"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/capture-tests"
                "-DCASE=${case_name}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/capture_cli_case.cmake"
        )
    endforeach()
    foreach(case_name invalid_limit limit_too_high limit_overflow limit_nonnumeric limit_plus_sign limit_minus_sign limit_whitespace limit_minimum limit_maximum missing_file directory_path empty_file malformed_file)
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
