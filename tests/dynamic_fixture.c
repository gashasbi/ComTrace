#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <evntrace.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dynamic_fixture.h"

static const GUID session_id = {
    0x3d9f6c52, 0x6e40, 0x4b83,
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
    unsigned short count = 3U;
    int values[3] = { 11, 22, 33 };
    unsigned short length = 4U;
    unsigned char data[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
    ULONG hex_count = 2U;
    int hex_values[2] = { 44, 55 };
    unsigned short text_length = 6U;
    wchar_t text[] = L"Hello";
    int registered = 0;
    int result = 1;

    if (argc != 2) {
        fwprintf(stderr, L"Usage: ComTraceDynamicFixture <etl-path>\n");
        return 1;
    }

    if (_snwprintf_s(
            logger_name,
            sizeof(logger_name) / sizeof(logger_name[0]),
            _TRUNCATE,
            L"ComTraceDynamicFixture_%lu",
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

    status = EventRegisterComTraceDynamicFixture();
    if (status != ERROR_SUCCESS) {
        fwprintf(stderr, L"EventRegister failed: %lu\n", (unsigned long)status);
        goto cleanup;
    }
    registered = 1;

    status = EnableTraceEx2(
        session,
        &COMTRACE_DYNAMIC_PROVIDER,
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

    status = EventWriteDynamicEvent(count, values, length, data, text_length, text, hex_count, hex_values);
    if (status != ERROR_SUCCESS) {
        fwprintf(stderr, L"EventWriteDynamicEvent failed: %lu\n", (unsigned long)status);
        goto cleanup;
    }

    result = 0;

cleanup:
    if (registered) {
        EventUnregisterComTraceDynamicFixture();
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
