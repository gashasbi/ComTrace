#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <evntrace.h>
#include <evntprov.h>
#include <stdio.h>
#include <limits.h>
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

static const GUID activity_id = {
    0x20314253, 0x6475, 0x8697,
    { 0xa8, 0xb9, 0xca, 0xdb, 0xec, 0xfd, 0x0e, 0x1f }
};

static const GUID related_activity_id = {
    0x30415263, 0x7485, 0x96a7,
    { 0xb8, 0xc9, 0xda, 0xeb, 0xfc, 0x0d, 0x1e, 0x2f }
};

static const GUID interface_pointer_id = {
    0x40516273, 0x8495, 0xa6b7,
    { 0xc8, 0xd9, 0xea, 0xfb, 0x0c, 0x1d, 0x2e, 0x3f }
};

static const GUID rpc_interface_uuid = {
    0x50617283, 0x94a5, 0xb6c7,
    { 0xd8, 0xe9, 0xfa, 0x0b, 0x1c, 0x2d, 0x3e, 0x4f }
};

static const GUID rpcss_interface_uuid = {
    0x60718293, 0xa4b5, 0xc6d7,
    { 0xe8, 0xf9, 0x0a, 0x1b, 0x2c, 0x3d, 0x4e, 0x5f }
};

static const GUID rpcss_object_uuid = {
    0x708192a3, 0xb4c5, 0xd6e7,
    { 0xf8, 0x09, 0x1a, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f }
};

static const GUID type_manager_uuid = {
    0x8091a2b3, 0xc4d5, 0xe6f7,
    { 0x08, 0x19, 0x2a, 0x3b, 0x4c, 0x5d, 0x6e, 0x7f }
};

static const GUID empty_activity_id = { 0 };

static const ULONGLONG object_id = 0x1122334455667788ULL;
static const ULONGLONG call_trace_id = 0x8877665544332211ULL;

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
    EVENT_DATA_DESCRIPTOR data[11];
    EVENT_DATA_DESCRIPTOR class_id_alias_data;
    EVENT_DATA_DESCRIPTOR call_trace_data;
    EVENT_DATA_DESCRIPTOR related_data;
    EVENT_DATA_DESCRIPTOR rundown_data[3];
    EVENT_DATA_DESCRIPTOR rpc_data[4];
    ULONG call_trace_id_32 = 0x12345678UL;
    ULONG related_marker = 456U;
    ULONG target_process_id = 0x12345678UL;
    ULONG target_thread_id = 0x87654321UL;
    ULONG source_process_id = 0x76543210UL;
    ULONG source_thread_id = 0x10203040UL;
    LONG hresult = (LONG)0x80004005UL;
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

    if (properties_size > (size_t)ULONG_MAX) {
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
    EventDataDescCreate(&data[2], &target_process_id, (ULONG)sizeof(target_process_id));
    EventDataDescCreate(&data[3], &target_thread_id, (ULONG)sizeof(target_thread_id));
    EventDataDescCreate(&data[4], (PVOID)&interface_pointer_id, (ULONG)sizeof(interface_pointer_id));
    EventDataDescCreate(&data[5], (PVOID)&object_id, (ULONG)sizeof(object_id));
    EventDataDescCreate(&data[6], (PVOID)&call_trace_id, (ULONG)sizeof(call_trace_id));
    EventDataDescCreate(&data[7], &hresult, (ULONG)sizeof(hresult));
    EventDataDescCreate(&data[8], &not_guid, (ULONG)sizeof(not_guid));
    EventDataDescCreate(&data[9], &source_process_id, (ULONG)sizeof(source_process_id));
    EventDataDescCreate(&data[10], &source_thread_id, (ULONG)sizeof(source_thread_id));

    status = EventWriteTransfer(
        provider,
        &descriptor,
        &activity_id,
        &related_activity_id,
        11U,
        data
    );
    if (status != ERROR_SUCCESS) {
        fwprintf(stderr, L"EventWrite failed: %lu\n", (unsigned long)status);
        goto cleanup;
    }

    descriptor.Id = 2U;
    EventDataDescCreate(
        &class_id_alias_data,
        (PVOID)&class_id,
        (ULONG)sizeof(class_id)
    );
    status = EventWrite(provider, &descriptor, 1U, &class_id_alias_data);
    if (status != ERROR_SUCCESS) {
        fwprintf(stderr, L"CLSID alias event write failed: %lu\n", (unsigned long)status);
        goto cleanup;
    }

    descriptor.Id = 3U;
    EventDataDescCreate(
        &call_trace_data,
        &call_trace_id_32,
        (ULONG)sizeof(call_trace_id_32)
    );
    status = EventWrite(
        provider,
        &descriptor,
        1U,
        &call_trace_data
    );
    if (status != ERROR_SUCCESS) {
        fwprintf(stderr, L"32-bit CallTraceId event write failed: %lu\n", (unsigned long)status);
        goto cleanup;
    }

    descriptor.Id = 4U;
    EventDataDescCreate(&related_data, &related_marker, (ULONG)sizeof(related_marker));
    status = EventWriteTransfer(
        provider,
        &descriptor,
        &empty_activity_id,
        &related_activity_id,
        1U,
        &related_data
    );
    if (status != ERROR_SUCCESS) {
        fwprintf(stderr, L"Related-only activity event write failed: %lu\n", (unsigned long)status);
        goto cleanup;
    }

    descriptor.Id = 5U;
    EventDataDescCreate(&rundown_data[0], (PVOID)&object_id, (ULONG)sizeof(object_id));
    EventDataDescCreate(&rundown_data[1], (PVOID)&interface_id, (ULONG)sizeof(interface_id));
    EventDataDescCreate(&rundown_data[2], (PVOID)&interface_pointer_id, (ULONG)sizeof(interface_pointer_id));
    status = EventWrite(provider, &descriptor, 3U, rundown_data);
    if (status != ERROR_SUCCESS) {
        fwprintf(stderr, L"Rundown identity event write failed: %lu\n", (unsigned long)status);
        goto cleanup;
    }

    descriptor.Id = 6U;
    EventDataDescCreate(&rpc_data[0], (PVOID)&rpc_interface_uuid, (ULONG)sizeof(rpc_interface_uuid));
    EventDataDescCreate(&rpc_data[1], (PVOID)&rpcss_interface_uuid, (ULONG)sizeof(rpcss_interface_uuid));
    EventDataDescCreate(&rpc_data[2], (PVOID)&rpcss_object_uuid, (ULONG)sizeof(rpcss_object_uuid));
    EventDataDescCreate(&rpc_data[3], (PVOID)&type_manager_uuid, (ULONG)sizeof(type_manager_uuid));
    status = EventWrite(provider, &descriptor, 4U, rpc_data);
    if (status != ERROR_SUCCESS) {
        fwprintf(stderr, L"RPC identity event write failed: %lu\n", (unsigned long)status);
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
