#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <evntrace.h>
#include <evntprov.h>
#include <TraceLoggingProvider.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "struct_fixture.h"

TRACELOGGING_DEFINE_PROVIDER(
    g_hNestedProvider,
    "ComTraceNestedFixture",
    (0x5a7b9c1d, 0x2e3f, 0x4a50, 0x8b, 0x6c, 0x7d, 0x8e, 0x9f, 0xa0, 0xb1, 0xc2)
);

static const GUID session_id = {
    0x2c8e7b41, 0x5d3f, 0x4a72,
    { 0x8b, 0x32, 0x74, 0x85, 0x96, 0xa7, 0xb8, 0xc9 }
};

int wmain(int argc, wchar_t **argv)
{
    BYTE storage[sizeof(EVENT_TRACE_PROPERTIES) + 256U + MAX_PATH * sizeof(wchar_t)];
    EVENT_TRACE_PROPERTIES *properties = (EVENT_TRACE_PROPERTIES *)storage;
    TRACEHANDLE session = 0;
    wchar_t logger_name[96];
    size_t logger_size;
    size_t path_size;
    ULONG path_offset;
    ULONG status;
    ULONG stop_status;
    REGHANDLE registration_handle = 0;
    GUID nested_provider_id;
    int tracelogging_registered = 0;
    EVENT_DATA_DESCRIPTOR descriptors[9];
    USHORT values_count = 2U;
    ULONG first_code = 0x11223344UL;
    USHORT first_status = 0x5566U;
    ULONG second_code = 0x55667788UL;
    USHORT second_status = 0x99AAU;
    ULONG fixed_first_code = 0xA1B2C3D4UL;
    USHORT fixed_first_status = 0x1234U;
    ULONG fixed_second_code = 0x10203040UL;
    USHORT fixed_second_status = 0x5678U;
    int result = 1;

    if (argc != 2) {
        fwprintf(stderr, L"Usage: ComTraceStructFixture <etl-path>\n");
        return 1;
    }

    if (_snwprintf_s(
            logger_name,
            sizeof(logger_name) / sizeof(logger_name[0]),
            _TRUNCATE,
            L"ComTraceStructFixture_%lu",
            (unsigned long)GetCurrentProcessId()) < 0) {
        return 1;
    }

    logger_size = (wcslen(logger_name) + 1U) * sizeof(wchar_t);
    path_size = (wcslen(argv[1]) + 1U) * sizeof(wchar_t);
    path_offset = (ULONG)sizeof(EVENT_TRACE_PROPERTIES) + (ULONG)logger_size;

    if (path_offset + path_size > sizeof(storage)) {
        return 1;
    }

    ZeroMemory(storage, sizeof(storage));
    properties->Wnode.BufferSize = (ULONG)(path_offset + path_size);
    properties->Wnode.Guid = session_id;
    properties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    properties->Wnode.ClientContext = 1U;
    properties->BufferSize = 64U;
    properties->MinimumBuffers = 2U;
    properties->MaximumBuffers = 8U;
    properties->LogFileMode = EVENT_TRACE_FILE_MODE_SEQUENTIAL;
    properties->MaximumFileSize = 5U;
    properties->LoggerNameOffset = (ULONG)sizeof(EVENT_TRACE_PROPERTIES);
    properties->LogFileNameOffset = path_offset;

    CopyMemory(storage + properties->LoggerNameOffset, logger_name, logger_size);
    CopyMemory(storage + path_offset, argv[1], path_size);

    status = StartTraceW(&session, logger_name, properties);
    if (status != ERROR_SUCCESS) {
        fwprintf(stderr, L"StartTraceW failed: %lu\n", (unsigned long)status);
        goto cleanup;
    }

    status = EventRegister(&COMTRACE_STRUCT_PROVIDER, NULL, NULL, &registration_handle);
    if (status != ERROR_SUCCESS) {
        fwprintf(stderr, L"EventRegister failed: %lu\n", (unsigned long)status);
        goto cleanup;
    }

    status = EnableTraceEx2(
        session,
        &COMTRACE_STRUCT_PROVIDER,
        EVENT_CONTROL_CODE_ENABLE_PROVIDER,
        TRACE_LEVEL_VERBOSE,
        0ULL,
        0U,
        5000U,
        NULL);
    if (status != ERROR_SUCCESS) {
        fwprintf(stderr, L"EnableTraceEx2 failed: %lu\n", (unsigned long)status);
        goto cleanup;
    }

    EventDataDescCreate(&descriptors[0], &values_count, sizeof(values_count));
    EventDataDescCreate(&descriptors[1], &first_code, sizeof(first_code));
    EventDataDescCreate(&descriptors[2], &first_status, sizeof(first_status));
    EventDataDescCreate(&descriptors[3], &second_code, sizeof(second_code));
    EventDataDescCreate(&descriptors[4], &second_status, sizeof(second_status));
    EventDataDescCreate(&descriptors[5], &fixed_first_code, sizeof(fixed_first_code));
    EventDataDescCreate(&descriptors[6], &fixed_first_status, sizeof(fixed_first_status));
    EventDataDescCreate(&descriptors[7], &fixed_second_code, sizeof(fixed_second_code));
    EventDataDescCreate(&descriptors[8], &fixed_second_status, sizeof(fixed_second_status));
    status = EventWrite(registration_handle, &StructEvent, 9U, descriptors);
    if (status != ERROR_SUCCESS) {
        fwprintf(stderr, L"EventWriteStructEvent failed: %lu\n", (unsigned long)status);
        goto cleanup;
    }

    status = TraceLoggingRegister(g_hNestedProvider);
    if (status != ERROR_SUCCESS) {
        fwprintf(stderr, L"TraceLoggingRegister failed: %lu\n", (unsigned long)status);
        goto cleanup;
    }
    tracelogging_registered = 1;

    nested_provider_id = TraceLoggingProviderId(g_hNestedProvider);
    status = EnableTraceEx2(
        session,
        &nested_provider_id,
        EVENT_CONTROL_CODE_ENABLE_PROVIDER,
        TRACE_LEVEL_VERBOSE,
        0ULL,
        0U,
        5000U,
        NULL);
    if (status != ERROR_SUCCESS) {
        fwprintf(stderr, L"TraceLogging EnableTraceEx2 failed: %lu\n", (unsigned long)status);
        goto cleanup;
    }

    TraceLoggingWrite(
        g_hNestedProvider,
        "NestedEvent",
        TraceLoggingLevel(TRACE_LEVEL_VERBOSE),
        TraceLoggingStruct(2, "Outer"),
        TraceLoggingUInt32(0x11223344UL, "Code"),
        TraceLoggingStruct(2, "Inner"),
        TraceLoggingUInt32(0x55667788UL, "NestedCode"),
        TraceLoggingUInt16(0x99AAU, "NestedStatus")
    );

    result = 0;

cleanup:
    if (tracelogging_registered) {
        TraceLoggingUnregister(g_hNestedProvider);
    }

    if (registration_handle != 0) {
        EventUnregister(registration_handle);
    }

    if (session != 0) {
        stop_status = ControlTraceW(
            session,
            logger_name,
            properties,
            EVENT_TRACE_CONTROL_STOP);
        if (stop_status != ERROR_SUCCESS) {
            fwprintf(stderr, L"ControlTraceW failed: %lu\n", (unsigned long)stop_status);
            result = 1;
        }
    }

    return result;
}
