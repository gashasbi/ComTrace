#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <evntrace.h>
#include <evntprov.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "identity_fixture.h"

static const GUID session_id = {
    0x19a4d2c8, 0x6f31, 0x4e52,
    { 0x8a, 0x73, 0x24, 0x35, 0x46, 0x57, 0x68, 0x79 }
};

static const GUID class_id = {
    0x00112233, 0x4455, 0x6677,
    { 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff }
};

static const GUID interface_id = {
    0x10213243, 0x5465, 0x7687,
    { 0x98, 0xa9, 0xba, 0xcb, 0xdc, 0xed, 0xfe, 0x0f }
};

int wmain(int argc, wchar_t **argv)
{
    TRACEHANDLE session = 0;
    REGHANDLE provider = 0;
    EVENT_TRACE_PROPERTIES *properties = NULL;
    wchar_t logger_name[96];
    size_t logger_size;
    size_t path_size;
    size_t path_offset;
    size_t properties_size;
    ULONG status;
    ULONG stop_status;
    EVENT_DESCRIPTOR descriptor;
    EVENT_DATA_DESCRIPTOR data[3];
    ULONG not_guid = 123U;
    int result = 1;

    if (argc != 2) {
        fwprintf(stderr, L"Usage: ComTraceIdentityFixture <etl-path>\n");
        return 1;
    }

    if (_snwprintf_s(
            logger_name,
            _countof(logger_name),
            _TRUNCATE,
            L"ComTraceIdentity_%lu",
            (unsigned long)GetCurrentProcessId()) < 0) {
        return 1;
    }

    logger_size = (wcslen(logger_name) + 1U) * sizeof(wchar_t);
    path_size = (wcslen(argv[1]) + 1U) * sizeof(wchar_t);
    path_offset = sizeof(EVENT_TRACE_PROPERTIES) + logger_size;
    properties_size = path_offset + path_size;

    if (properties_size > MAXULONG) {
        return 1;
    }

    properties = (EVENT_TRACE_PROPERTIES *)calloc(1U, properties_size);
    if (properties == NULL) {
        return 1;
    }

    properties->Wnode.BufferSize = (ULONG)properties_size;
    properties->Wnode.Guid = session_id;
    properties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    properties->Wnode.ClientContext = 1U;
    properties->BufferSize = 64U;
    properties->MinimumBuffers = 2U;
    properties->MaximumBuffers = 8U;
    properties->LogFileMode = EVENT_TRACE_FILE_MODE_SEQUENTIAL;
    properties->MaximumFileSize = 5U;
    properties->LoggerNameOffset = (ULONG)sizeof(EVENT_TRACE_PROPERTIES);
    properties->LogFileNameOffset = (ULONG)path_offset;

    CopyMemory((BYTE *)properties + properties->LoggerNameOffset, logger_name, logger_size);
    CopyMemory((BYTE *)properties + properties->LogFileNameOffset, argv[1], path_size);

    status = StartTraceW(&session, logger_name, properties);
    if (status != ERROR_SUCCESS) {
        fwprintf(stderr, L"StartTraceW failed: %lu\n", (unsigned long)status);
        goto cleanup;
    }

    status = EventRegister(&COMTRACE_IDENTITY_PROVIDER, NULL, NULL, &provider);
    if (status != ERROR_SUCCESS) {
        fwprintf(stderr, L"EventRegister failed: %lu\n", (unsigned long)status);
        goto cleanup;
    }

    status = EnableTraceEx2(
        session,
        &COMTRACE_IDENTITY_PROVIDER,
        EVENT_CONTROL_CODE_ENABLE_PROVIDER,
        TRACE_LEVEL_VERBOSE,
        IDENTITY_KEYWORD,
        0U,
        5000U,
        NULL
    );
    if (status != ERROR_SUCCESS) {
        fwprintf(stderr, L"EnableTraceEx2 failed: %lu\n", (unsigned long)status);
        goto cleanup;
    }

    ZeroMemory(&descriptor, sizeof(descriptor));
    descriptor.Id = 1U;
    descriptor.Level = TRACE_LEVEL_INFORMATION;
    descriptor.Keyword = IDENTITY_KEYWORD;

    if (!EventEnabled(provider, &descriptor)) {
        fwprintf(stderr, L"Identity fixture provider is not enabled.\n");
        goto cleanup;
    }

    EventDataDescCreate(&data[0], (PVOID)&class_id, (ULONG)sizeof(class_id));
    EventDataDescCreate(&data[1], (PVOID)&interface_id, (ULONG)sizeof(interface_id));
    EventDataDescCreate(&data[2], &not_guid, (ULONG)sizeof(not_guid));

    status = EventWrite(provider, &descriptor, 3U, data);
    if (status != ERROR_SUCCESS) {
        fwprintf(stderr, L"EventWrite failed: %lu\n", (unsigned long)status);
        goto cleanup;
    }

    result = 0;

cleanup:
    if (provider != 0) {
            (void)EventUnregister(provider);
    }

    if (session != 0) {
        stop_status = ControlTraceW(
            session,
            logger_name,
            properties,
            EVENT_TRACE_CONTROL_STOP
        );
        if (stop_status != ERROR_SUCCESS) {
            fwprintf(stderr, L"ControlTraceW failed: %lu\n", (unsigned long)stop_status);
            result = 1;
        }
    }

    free(properties);
    return result;
}
