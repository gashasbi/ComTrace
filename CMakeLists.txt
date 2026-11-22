cmake_minimum_required(VERSION 3.21)

cmake_policy(SET CMP0091 NEW)

project(ComTrace VERSION 0.2.0 LANGUAGES C RC)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_executable(ComTrace
    src/main.c
    src/etw_capture.c
    src/etl_inspect.c
    src/etl_decoder.c
    src/etl_value.c
    src/com_event.c
    src/com_lifetime_analysis.c
    src/ole_event.c
    src/event_record.c
    src/event_catalog.c
    src/com_identity.c
    src/process_lifecycle.c
    src/com_timeline.c
    src/relationship_graph.c
    src/ole_timeline.c
    src/object_identity_compare.c
    src/trace_compare.c
    src/archive_reader.c
    src/archive_checksum.c
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

add_library(ComTraceLifetimeInstrumentation STATIC
    src/com_lifetime.c
)
target_include_directories(ComTraceLifetimeInstrumentation PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_link_libraries(ComTraceLifetimeInstrumentation PUBLIC Advapi32)

if(MSVC)
    target_compile_options(ComTraceLifetimeInstrumentation PRIVATE /W4 /WX /wd5105 /utf-8)
else()
    target_compile_options(ComTraceLifetimeInstrumentation PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_executable(ComTraceComLifetimeFixture tests/com_lifetime_fixture.c)
target_link_libraries(ComTraceComLifetimeFixture PRIVATE ComTraceLifetimeInstrumentation)

if(MSVC)
    target_compile_options(ComTraceComLifetimeFixture PRIVATE /W4 /WX /wd5105 /utf-8)
else()
    target_compile_options(ComTraceComLifetimeFixture PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

add_executable(ComTraceArchive
    src/archive_main.c
    src/etl_archive.c
    src/archive_writer.c
    src/archive_reader.c
    src/archive_checksum.c
)

target_include_directories(ComTraceArchive PRIVATE src)
target_link_libraries(ComTraceArchive PRIVATE Advapi32)

if(MINGW)
    target_link_options(ComTraceArchive PRIVATE -municode)
endif()

if(MSVC)
    target_compile_options(ComTraceArchive PRIVATE /W4 /WX /wd5105 /utf-8)
else()
    target_compile_options(ComTraceArchive PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()
include(CTest)

if(BUILD_TESTING)
    file(GLOB COMTRACE_WINDOWS_SDK_BIN_DIRS "C:/Program Files (x86)/Windows Kits/10/bin/*/x64")
    list(SORT COMTRACE_WINDOWS_SDK_BIN_DIRS COMPARE NATURAL ORDER DESCENDING)
    find_program(COMTRACE_MC_EXECUTABLE mc.exe HINTS ${COMTRACE_WINDOWS_SDK_BIN_DIRS})
    if(NOT COMTRACE_MC_EXECUTABLE)
        message(FATAL_ERROR "Message Compiler mc.exe was not found.")
    endif()

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

    add_executable(ComTraceComLifetimeAnalysisTests
        tests/test_com_lifetime_analysis.c
        src/com_lifetime_analysis.c
    )
    target_include_directories(ComTraceComLifetimeAnalysisTests PRIVATE src)

    if(MSVC)
        target_compile_options(ComTraceComLifetimeAnalysisTests PRIVATE /W4 /WX /wd5105 /utf-8)
    else()
        target_compile_options(ComTraceComLifetimeAnalysisTests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()

    add_test(NAME comtrace_com_lifetime_analysis COMMAND ComTraceComLifetimeAnalysisTests)


    add_test(
        NAME comtrace_com_lifetime_capture
        COMMAND "${CMAKE_COMMAND}"
            "-DCOMTRACE_EXE=$<TARGET_FILE:ComTrace>"
            "-DFIXTURE_EXE=$<TARGET_FILE:ComTraceComLifetimeFixture>"
            "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/com-lifetime-tests"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/com_lifetime_case.cmake"
    )
    set_tests_properties(comtrace_com_lifetime_capture PROPERTIES RUN_SERIAL TRUE)

    add_executable(ComTraceTimelineTests
        tests/test_com_timeline.c
        src/com_timeline.c
    )
    target_include_directories(ComTraceTimelineTests PRIVATE src)

    if(MSVC)
        target_compile_options(ComTraceTimelineTests PRIVATE /W4 /WX /wd5105 /utf-8)
    else()
        target_compile_options(ComTraceTimelineTests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()

    add_test(NAME comtrace_timeline COMMAND ComTraceTimelineTests)

    add_executable(ComTraceRelationshipGraphTests
        tests/test_relationship_graph.c
        src/relationship_graph.c
        src/process_lifecycle.c
        src/com_identity.c
        src/etl_decoder.c
    )
    target_include_directories(ComTraceRelationshipGraphTests PRIVATE src)
    target_link_libraries(ComTraceRelationshipGraphTests PRIVATE Tdh Advapi32)

    if(MSVC)
        target_compile_options(ComTraceRelationshipGraphTests PRIVATE /W4 /WX /wd5105 /utf-8)
    else()
        target_compile_options(ComTraceRelationshipGraphTests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()

    add_test(NAME comtrace_relationship_graph COMMAND ComTraceRelationshipGraphTests)

    add_executable(ComTraceObjectIdentityCompareTests
        tests/test_object_identity_compare.c
        src/object_identity_compare.c
        src/relationship_graph.c
        src/process_lifecycle.c
        src/com_identity.c
        src/etl_decoder.c
    )
    target_include_directories(ComTraceObjectIdentityCompareTests PRIVATE src)
    target_link_libraries(ComTraceObjectIdentityCompareTests PRIVATE Tdh Advapi32)

    if(MSVC)
        target_compile_options(ComTraceObjectIdentityCompareTests PRIVATE /W4 /WX /wd5105 /utf-8)
    else()
        target_compile_options(ComTraceObjectIdentityCompareTests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()

    add_test(NAME comtrace_object_identity_compare COMMAND ComTraceObjectIdentityCompareTests)

    add_executable(ComTraceOleTimelineTests
        tests/test_ole_timeline.c
        src/ole_timeline.c
    )
    target_include_directories(ComTraceOleTimelineTests PRIVATE src)

    if(MSVC)
        target_compile_options(ComTraceOleTimelineTests PRIVATE /W4 /WX /wd5105 /utf-8)
    else()
        target_compile_options(ComTraceOleTimelineTests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()

    add_test(NAME comtrace_ole_timeline COMMAND ComTraceOleTimelineTests)

    add_executable(ComTraceProcessLifecycleTests
        tests/test_process_lifecycle.c
        src/process_lifecycle.c
        src/com_identity.c
    )
    target_include_directories(ComTraceProcessLifecycleTests PRIVATE src)
    target_link_libraries(ComTraceProcessLifecycleTests PRIVATE Tdh Advapi32)

    if(MSVC)
        target_compile_options(ComTraceProcessLifecycleTests PRIVATE /W4 /WX /wd5105 /utf-8)
    else()
        target_compile_options(ComTraceProcessLifecycleTests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()

    add_test(NAME comtrace_process_lifecycle COMMAND ComTraceProcessLifecycleTests)

    add_executable(ComTraceComIdentityTests
        tests/test_com_identity.c
        src/com_identity.c
        src/etl_decoder.c
    )
    target_include_directories(ComTraceComIdentityTests PRIVATE src)
    target_link_libraries(ComTraceComIdentityTests PRIVATE Tdh Advapi32)

    if(MSVC)
        target_compile_options(ComTraceComIdentityTests PRIVATE /W4 /WX /wd5105 /utf-8)
    else()
        target_compile_options(ComTraceComIdentityTests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()

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

    add_executable(ComTraceEventRecordTests
        tests/test_event_record.c
        src/event_record.c
    )

    target_include_directories(ComTraceEventRecordTests PRIVATE src)

    if(MSVC)
        target_compile_options(ComTraceEventRecordTests PRIVATE /W4 /WX /wd5105 /utf-8)
    else()
        target_compile_options(ComTraceEventRecordTests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()

    add_test(NAME comtrace_event_record COMMAND ComTraceEventRecordTests)

    add_executable(ComTraceEventCatalogTests
        tests/test_event_catalog.c
        src/event_catalog.c
    )

    target_include_directories(ComTraceEventCatalogTests PRIVATE src)

    if(MSVC)
        target_compile_options(ComTraceEventCatalogTests PRIVATE /W4 /WX /wd5105 /utf-8)
    else()
        target_compile_options(ComTraceEventCatalogTests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()

    add_test(NAME comtrace_event_catalog COMMAND ComTraceEventCatalogTests)

    add_executable(ComTraceArchiveChecksumTests
        tests/test_archive_checksum.c
        src/archive_checksum.c
    )
    target_include_directories(ComTraceArchiveChecksumTests PRIVATE src)

    if(MSVC)
        target_compile_options(ComTraceArchiveChecksumTests PRIVATE /W4 /WX /wd5105 /utf-8)
    else()
        target_compile_options(ComTraceArchiveChecksumTests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()

    add_test(NAME comtrace_archive_checksum COMMAND ComTraceArchiveChecksumTests)

    add_executable(ComTraceArchiveWriterTests
        tests/test_archive_writer.c
        src/archive_writer.c
    )

    target_include_directories(ComTraceArchiveWriterTests PRIVATE src)
    target_compile_definitions(ComTraceArchiveWriterTests PRIVATE COMTRACE_ARCHIVE_WRITER_TESTING)

    if(MSVC)
        target_compile_options(ComTraceArchiveWriterTests PRIVATE /W4 /WX /wd5105 /utf-8)
    else()
        target_compile_options(ComTraceArchiveWriterTests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()

    add_test(NAME comtrace_archive_writer COMMAND ComTraceArchiveWriterTests)

    add_executable(ComTraceArchiveEtlTests
        tests/test_archive_etl.c
        src/etl_archive.c
        src/archive_writer.c
        src/archive_reader.c
    )
    target_include_directories(ComTraceArchiveEtlTests PRIVATE src)
    target_link_libraries(ComTraceArchiveEtlTests PRIVATE Advapi32)
    if(MSVC)
        target_compile_options(ComTraceArchiveEtlTests PRIVATE /W4 /WX /wd5105 /utf-8)
    else()
        target_compile_options(ComTraceArchiveEtlTests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(NAME comtrace_archive_etl COMMAND ComTraceArchiveEtlTests)

    add_executable(ComTraceArchiveReaderTests
        tests/test_archive_reader.c
        src/archive_reader.c
        src/archive_writer.c
    )

    target_include_directories(ComTraceArchiveReaderTests PRIVATE src)
    target_link_libraries(ComTraceArchiveReaderTests PRIVATE Advapi32)

    if(MSVC)
        target_compile_options(ComTraceArchiveReaderTests PRIVATE /W4 /WX /wd5105 /utf-8)
    else()
        target_compile_options(ComTraceArchiveReaderTests PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()

    add_test(NAME comtrace_archive_reader COMMAND ComTraceArchiveReaderTests)

    add_executable(ComTraceArchiveTestFixture tests/archive_cli_fixture.c src/archive_writer.c)
    target_include_directories(ComTraceArchiveTestFixture PRIVATE src)
    target_link_libraries(ComTraceArchiveTestFixture PRIVATE Advapi32)

    if(MSVC)
        target_compile_options(ComTraceArchiveTestFixture PRIVATE /W4 /WX /wd5105 /utf-8)
    else()
        target_compile_options(ComTraceArchiveTestFixture PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()

    foreach(case_name help version no_arguments invalid_arguments missing_input inspect_valid recover_valid)
        add_test(
            NAME comtrace_archive_cli_${case_name}
            COMMAND "${CMAKE_COMMAND}"
                "-DCOMTRACE_ARCHIVE_EXE=$<TARGET_FILE:ComTraceArchive>"
                "-DCOMTRACE_ARCHIVE_FIXTURE_EXE=$<TARGET_FILE:ComTraceArchiveTestFixture>"
                "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/archive-cli-tests"
                "-DCASE=${case_name}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/archive_cli_case.cmake"
        )
    endforeach()
    add_test(
        NAME comtrace_compare_json
        COMMAND "${CMAKE_COMMAND}"
            "-DCOMTRACE_EXE=$<TARGET_FILE:ComTrace>"
            "-DFIXTURE_EXE=$<TARGET_FILE:ComTraceArchiveTestFixture>"
            "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/compare-json-tests"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/compare_json_case.cmake"
    )
    add_executable(ComTraceArrayFixture tests/array_fixture.c)
    target_link_libraries(ComTraceArrayFixture PRIVATE Advapi32)
    set(COMTRACE_ARRAY_FIXTURE_DIR "${CMAKE_CURRENT_BINARY_DIR}/array-fixture/$<CONFIG>")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/tests/array_fixture.man" COMTRACE_ARRAY_MANIFEST_CONTENT)
    string(REPLACE "COMTRACE_ARRAY_FIXTURE_PATH" "$<TARGET_FILE:ComTraceArrayFixture>" COMTRACE_ARRAY_MANIFEST_CONTENT "${COMTRACE_ARRAY_MANIFEST_CONTENT}")
    file(GENERATE OUTPUT "${COMTRACE_ARRAY_FIXTURE_DIR}/array_fixture.man" CONTENT "${COMTRACE_ARRAY_MANIFEST_CONTENT}")
    set(COMTRACE_ARRAY_GENERATED_FILES
        "${COMTRACE_ARRAY_FIXTURE_DIR}/array_fixture.h"
        "${COMTRACE_ARRAY_FIXTURE_DIR}/array_fixture.rc"
        "${COMTRACE_ARRAY_FIXTURE_DIR}/array_fixtureTEMP.BIN"
        "${COMTRACE_ARRAY_FIXTURE_DIR}/MSG00001.bin"
    )
    add_custom_command(
        OUTPUT ${COMTRACE_ARRAY_GENERATED_FILES}
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${COMTRACE_ARRAY_FIXTURE_DIR}"
        COMMAND "${COMTRACE_MC_EXECUTABLE}" -um -h "${COMTRACE_ARRAY_FIXTURE_DIR}" -r "${COMTRACE_ARRAY_FIXTURE_DIR}" "${COMTRACE_ARRAY_FIXTURE_DIR}/array_fixture.man"
        DEPENDS "${COMTRACE_ARRAY_FIXTURE_DIR}/array_fixture.man" "${CMAKE_CURRENT_SOURCE_DIR}/tests/array_fixture.man"
        VERBATIM
    )
    target_sources(ComTraceArrayFixture PRIVATE "${COMTRACE_ARRAY_FIXTURE_DIR}/array_fixture.rc" "${COMTRACE_ARRAY_FIXTURE_DIR}/array_fixture.h")
    target_include_directories(ComTraceArrayFixture PRIVATE "${COMTRACE_ARRAY_FIXTURE_DIR}")
    if(MSVC)
        target_compile_options(ComTraceArrayFixture PRIVATE /W4 /WX /wd5105 /utf-8)
    else()
        target_compile_options(ComTraceArrayFixture PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(
        NAME comtrace_inspect_fixed_array
        COMMAND "${CMAKE_COMMAND}"
            "-DCOMTRACE_EXE=$<TARGET_FILE:ComTrace>"
            "-DFIXTURE_EXE=$<TARGET_FILE:ComTraceArrayFixture>"
            "-DMANIFEST_FILE=${COMTRACE_ARRAY_FIXTURE_DIR}/array_fixture.man"
            "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/array-cli-tests"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/array_cli_case.cmake"
    )
    set_tests_properties(comtrace_inspect_fixed_array PROPERTIES RUN_SERIAL TRUE)
    add_executable(ComTraceStructFixture tests/struct_fixture.c)
    target_link_libraries(ComTraceStructFixture PRIVATE Advapi32)
    set(COMTRACE_STRUCT_FIXTURE_DIR "${CMAKE_CURRENT_BINARY_DIR}/struct-fixture/$<CONFIG>")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/tests/struct_fixture.man" COMTRACE_STRUCT_MANIFEST_CONTENT)
    string(REPLACE "COMTRACE_STRUCT_FIXTURE_PATH" "$<TARGET_FILE:ComTraceStructFixture>" COMTRACE_STRUCT_MANIFEST_CONTENT "${COMTRACE_STRUCT_MANIFEST_CONTENT}")
    file(GENERATE OUTPUT "${COMTRACE_STRUCT_FIXTURE_DIR}/struct_fixture.man" CONTENT "${COMTRACE_STRUCT_MANIFEST_CONTENT}")
    set(COMTRACE_STRUCT_GENERATED_FILES
        "${COMTRACE_STRUCT_FIXTURE_DIR}/struct_fixture.h"
        "${COMTRACE_STRUCT_FIXTURE_DIR}/struct_fixture.rc"
        "${COMTRACE_STRUCT_FIXTURE_DIR}/struct_fixtureTEMP.BIN"
        "${COMTRACE_STRUCT_FIXTURE_DIR}/MSG00001.bin"
    )
    add_custom_command(
        OUTPUT ${COMTRACE_STRUCT_GENERATED_FILES}
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${COMTRACE_STRUCT_FIXTURE_DIR}"
        COMMAND "${COMTRACE_MC_EXECUTABLE}" -um -h "${COMTRACE_STRUCT_FIXTURE_DIR}" -r "${COMTRACE_STRUCT_FIXTURE_DIR}" "${COMTRACE_STRUCT_FIXTURE_DIR}/struct_fixture.man"
        DEPENDS "${COMTRACE_STRUCT_FIXTURE_DIR}/struct_fixture.man" "${CMAKE_CURRENT_SOURCE_DIR}/tests/struct_fixture.man"
        VERBATIM
    )
    target_sources(ComTraceStructFixture PRIVATE "${COMTRACE_STRUCT_FIXTURE_DIR}/struct_fixture.rc" "${COMTRACE_STRUCT_FIXTURE_DIR}/struct_fixture.h")
    target_include_directories(ComTraceStructFixture PRIVATE "${COMTRACE_STRUCT_FIXTURE_DIR}")
    if(MSVC)
        target_compile_options(ComTraceStructFixture PRIVATE /W4 /WX /wd5105 /utf-8)
    else()
        target_compile_options(ComTraceStructFixture PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(
        NAME comtrace_inspect_struct
        COMMAND "${CMAKE_COMMAND}"
            "-DCOMTRACE_EXE=$<TARGET_FILE:ComTrace>"
            "-DFIXTURE_EXE=$<TARGET_FILE:ComTraceStructFixture>"
            "-DMANIFEST_FILE=${COMTRACE_STRUCT_FIXTURE_DIR}/struct_fixture.man"
            "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/struct-cli-tests"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/struct_cli_case.cmake"
    )
    set_tests_properties(comtrace_inspect_struct PROPERTIES RUN_SERIAL TRUE)
    add_executable(ComTraceIdentityFixture tests/identity_fixture.c)
    target_link_libraries(ComTraceIdentityFixture PRIVATE Advapi32)
    set(COMTRACE_IDENTITY_FIXTURE_DIR "${CMAKE_CURRENT_BINARY_DIR}/identity-fixture/$<CONFIG>")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/tests/identity_fixture.man" COMTRACE_IDENTITY_MANIFEST_CONTENT)
    string(REPLACE "COMTRACE_IDENTITY_FIXTURE_PATH" "$<TARGET_FILE:ComTraceIdentityFixture>" COMTRACE_IDENTITY_MANIFEST_CONTENT "${COMTRACE_IDENTITY_MANIFEST_CONTENT}")
    file(GENERATE OUTPUT "${COMTRACE_IDENTITY_FIXTURE_DIR}/identity_fixture.man" CONTENT "${COMTRACE_IDENTITY_MANIFEST_CONTENT}")
    set(COMTRACE_IDENTITY_GENERATED_FILES
        "${COMTRACE_IDENTITY_FIXTURE_DIR}/identity_fixture.h"
        "${COMTRACE_IDENTITY_FIXTURE_DIR}/identity_fixture.rc"
        "${COMTRACE_IDENTITY_FIXTURE_DIR}/identity_fixtureTEMP.BIN"
        "${COMTRACE_IDENTITY_FIXTURE_DIR}/MSG00001.bin"
    )
    add_custom_command(
        OUTPUT ${COMTRACE_IDENTITY_GENERATED_FILES}
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${COMTRACE_IDENTITY_FIXTURE_DIR}"
        COMMAND "${COMTRACE_MC_EXECUTABLE}" -um -h "${COMTRACE_IDENTITY_FIXTURE_DIR}" -r "${COMTRACE_IDENTITY_FIXTURE_DIR}" "${COMTRACE_IDENTITY_FIXTURE_DIR}/identity_fixture.man"
        DEPENDS "${COMTRACE_IDENTITY_FIXTURE_DIR}/identity_fixture.man" "${CMAKE_CURRENT_SOURCE_DIR}/tests/identity_fixture.man"
        VERBATIM
    )
    target_sources(ComTraceIdentityFixture PRIVATE "${COMTRACE_IDENTITY_FIXTURE_DIR}/identity_fixture.rc" "${COMTRACE_IDENTITY_FIXTURE_DIR}/identity_fixture.h")
    target_include_directories(ComTraceIdentityFixture PRIVATE "${COMTRACE_IDENTITY_FIXTURE_DIR}")
    if(MSVC)
        target_compile_options(ComTraceIdentityFixture PRIVATE /W4 /WX /wd5105 /utf-8)
    else()
        target_compile_options(ComTraceIdentityFixture PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(
        NAME comtrace_com_identity
        COMMAND "${CMAKE_COMMAND}"
            "-DFIXTURE_EXE=$<TARGET_FILE:ComTraceIdentityFixture>"
            "-DTEST_EXE=$<TARGET_FILE:ComTraceComIdentityTests>"
            "-DCOMTRACE_EXE=$<TARGET_FILE:ComTrace>"
            "-DCOMTRACE_ARCHIVE_EXE=$<TARGET_FILE:ComTraceArchive>"
            "-DMANIFEST_FILE=${COMTRACE_IDENTITY_FIXTURE_DIR}/identity_fixture.man"
            "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/identity-cli-tests"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/com_identity_case.cmake"
    )
    set_tests_properties(comtrace_com_identity PROPERTIES RUN_SERIAL TRUE)
    add_executable(ComTraceDynamicFixture tests/dynamic_fixture.c)
    target_link_libraries(ComTraceDynamicFixture PRIVATE Advapi32)
    set(COMTRACE_DYNAMIC_FIXTURE_DIR "${CMAKE_CURRENT_BINARY_DIR}/dynamic-fixture/$<CONFIG>")
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/tests/dynamic_fixture.man" COMTRACE_DYNAMIC_MANIFEST_CONTENT)
    string(REPLACE "COMTRACE_DYNAMIC_FIXTURE_PATH" "$<TARGET_FILE:ComTraceDynamicFixture>" COMTRACE_DYNAMIC_MANIFEST_CONTENT "${COMTRACE_DYNAMIC_MANIFEST_CONTENT}")
    file(GENERATE OUTPUT "${COMTRACE_DYNAMIC_FIXTURE_DIR}/dynamic_fixture.man" CONTENT "${COMTRACE_DYNAMIC_MANIFEST_CONTENT}")
    set(COMTRACE_DYNAMIC_GENERATED_FILES
        "${COMTRACE_DYNAMIC_FIXTURE_DIR}/dynamic_fixture.h"
        "${COMTRACE_DYNAMIC_FIXTURE_DIR}/dynamic_fixture.rc"
        "${COMTRACE_DYNAMIC_FIXTURE_DIR}/dynamic_fixtureTEMP.BIN"
        "${COMTRACE_DYNAMIC_FIXTURE_DIR}/MSG00001.bin"
    )
    add_custom_command(
        OUTPUT ${COMTRACE_DYNAMIC_GENERATED_FILES}
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${COMTRACE_DYNAMIC_FIXTURE_DIR}"
        COMMAND "${COMTRACE_MC_EXECUTABLE}" -um -h "${COMTRACE_DYNAMIC_FIXTURE_DIR}" -r "${COMTRACE_DYNAMIC_FIXTURE_DIR}" "${COMTRACE_DYNAMIC_FIXTURE_DIR}/dynamic_fixture.man"
        DEPENDS "${COMTRACE_DYNAMIC_FIXTURE_DIR}/dynamic_fixture.man" "${CMAKE_CURRENT_SOURCE_DIR}/tests/dynamic_fixture.man"
        VERBATIM
    )
    target_sources(ComTraceDynamicFixture PRIVATE "${COMTRACE_DYNAMIC_FIXTURE_DIR}/dynamic_fixture.rc" "${COMTRACE_DYNAMIC_FIXTURE_DIR}/dynamic_fixture.h")
    target_include_directories(ComTraceDynamicFixture PRIVATE "${COMTRACE_DYNAMIC_FIXTURE_DIR}")
    if(MSVC)
        target_compile_options(ComTraceDynamicFixture PRIVATE /W4 /WX /wd5105 /utf-8)
    else()
        target_compile_options(ComTraceDynamicFixture PRIVATE -Wall -Wextra -Wpedantic -Werror)
    endif()
    add_test(
        NAME comtrace_inspect_dynamic_properties
        COMMAND "${CMAKE_COMMAND}"
            "-DCOMTRACE_EXE=$<TARGET_FILE:ComTrace>"
            "-DFIXTURE_EXE=$<TARGET_FILE:ComTraceDynamicFixture>"
            "-DMANIFEST_FILE=${COMTRACE_DYNAMIC_FIXTURE_DIR}/dynamic_fixture.man"
            "-DTEST_DIR=${CMAKE_CURRENT_BINARY_DIR}/dynamic-cli-tests"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/dynamic_cli_case.cmake"
    )
    set_tests_properties(comtrace_inspect_dynamic_properties PROPERTIES RUN_SERIAL TRUE)
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
    foreach(target
        ComTraceArchiveWriterTests
        ComTraceArchiveEtlTests
        ComTraceArchiveReaderTests
        ComTraceArchiveTestFixture
    )
        target_sources(${target} PRIVATE src/archive_checksum.c)
    endforeach()
endif()

install(TARGETS ComTrace ComTraceArchive
    RUNTIME DESTINATION .
)
install(TARGETS ComTraceLifetimeInstrumentation
    ARCHIVE DESTINATION lib
)
install(FILES include/comtrace_lifetime.h DESTINATION include)
install(FILES README.md LICENSE DESTINATION .)
install(DIRECTORY docs DESTINATION .)
install(DIRECTORY scripts DESTINATION .)

add_executable(ComTraceOleActivityFixture tests/ole_activity_fixture.c)
target_link_libraries(ComTraceOleActivityFixture PRIVATE Ole32 Shell32)
install(TARGETS ComTraceOleActivityFixture RUNTIME DESTINATION .)

if(MSVC)
    target_compile_options(ComTraceOleActivityFixture PRIVATE /W4 /WX /wd5105 /utf-8)
else()
    target_compile_options(ComTraceOleActivityFixture PRIVATE -Wall -Wextra -Wpedantic -Werror)
endif()

set(CPACK_PACKAGE_NAME "ComTrace")
set(CPACK_PACKAGE_VENDOR "ComTrace")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Windows ETW COM/OLE trace tools")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "ComTrace-${PROJECT_VERSION}-windows-x64")
set(CPACK_GENERATOR "ZIP")
include(CPack)
