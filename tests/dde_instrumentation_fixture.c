#include <windows.h>
#include <evntrace.h>
#include <evntprov.h>
#include <ddeml.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include "comtrace_dde.h"

static const wchar_t trace_session_name[] = L"ComTrace DDE Fixture";

static const GUID trace_session_guid = {
    0x4b3f1a20,
    0x8c61,
    0x4d72,
    {0x91, 0xa3, 0x2b, 0x4c, 0x5d, 0x6e, 0x7f, 0x80}
};

static const GUID dde_provider_guid = {
    0x7b6d4a21,
    0x9e35,
    0x4c80,
    {0xa1, 0x72, 0x53, 0x94, 0xb6, 0xd8, 0x0f, 0x21}
};

static int start_trace_session(
    const wchar_t *etl_path,
    TRACEHANDLE *trace_handle,
    PEVENT_TRACE_PROPERTIES *properties_out
)
{
    SIZE_T logger_chars = _countof(trace_session_name);
    SIZE_T path_chars = wcslen(etl_path) + 1U;
    SIZE_T properties_size = sizeof(EVENT_TRACE_PROPERTIES) +
        (logger_chars + path_chars) * sizeof(wchar_t);
    PEVENT_TRACE_PROPERTIES properties;
    ULONG status;

    properties = (PEVENT_TRACE_PROPERTIES)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        properties_size
    );
    if (properties == NULL) {
        return 0;
    }

    properties->Wnode.BufferSize = (ULONG)properties_size;
    properties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    properties->Wnode.Guid = trace_session_guid;
    properties->Wnode.ClientContext = 1U;
    properties->BufferSize = 64U;
    properties->MinimumBuffers = 2U;
    properties->MaximumBuffers = 8U;
    properties->MaximumFileSize = 16U;
    properties->LogFileMode = EVENT_TRACE_FILE_MODE_SEQUENTIAL;
    properties->LoggerNameOffset = (ULONG)sizeof(EVENT_TRACE_PROPERTIES);
    properties->LogFileNameOffset = properties->LoggerNameOffset +
        (ULONG)(logger_chars * sizeof(wchar_t));

    CopyMemory(
        (BYTE *)properties + properties->LoggerNameOffset,
        trace_session_name,
        logger_chars * sizeof(wchar_t)
    );
    CopyMemory(
        (BYTE *)properties + properties->LogFileNameOffset,
        etl_path,
        path_chars * sizeof(wchar_t)
    );

    status = StartTraceW(trace_handle, trace_session_name, properties);
    if (status != ERROR_SUCCESS) {
        fwprintf(stderr, L"StartTraceW failed: %lu\n", (unsigned long)status);
        HeapFree(GetProcessHeap(), 0U, properties);
        return 0;
    }

    status = EnableTraceEx2(
        *trace_handle,
        &dde_provider_guid,
        EVENT_CONTROL_CODE_ENABLE_PROVIDER,
        TRACE_LEVEL_INFORMATION,
        0x1ULL,
        0U,
        0U,
        NULL
    );
    if (status != ERROR_SUCCESS) {
        fwprintf(stderr, L"EnableTraceEx2 failed: %lu\n", (unsigned long)status);
        (void)ControlTraceW(
            *trace_handle,
            trace_session_name,
            properties,
            EVENT_TRACE_CONTROL_STOP
        );
        HeapFree(GetProcessHeap(), 0U, properties);
        *trace_handle = 0U;
        return 0;
    }

    *properties_out = properties;
    return 1;
}

static int stop_trace_session(
    TRACEHANDLE trace_handle,
    PEVENT_TRACE_PROPERTIES properties
)
{
    ULONG status = ControlTraceW(
        trace_handle,
        trace_session_name,
        properties,
        EVENT_TRACE_CONTROL_STOP
    );

    if (status != ERROR_SUCCESS) {
        fwprintf(stderr, L"ControlTraceW failed: %lu\n", (unsigned long)status);
        return 0;
    }

    return 1;
}

int wmain(int argc, wchar_t **argv)
{
    int result = EXIT_FAILURE;
    ULONG status;
    TRACEHANDLE trace_handle = 0U;
    PEVENT_TRACE_PROPERTIES properties = NULL;
    ULONGLONG client_conversation_id = 0ULL;
    ULONGLONG server_conversation_id = 0ULL;
    ULONGLONG transaction_instance_id = 0ULL;
    const ULONGLONG conversation_correlation_id = 0x100000001ULL;
    const ULONGLONG transaction_correlation_id = 0x200000001ULL;
    BOOL provider_registered = FALSE;

    if (argc != 2 || argv[1] == NULL || argv[1][0] == L'\0') {
        fwprintf(stderr, L"Usage: ComTraceDdeInstrumentationFixture <output.etl>\n");
        return EXIT_FAILURE;
    }

    if (!start_trace_session(argv[1], &trace_handle, &properties)) {
        goto cleanup;
    }

    status = comtrace_dde_initialize();
    if (status != ERROR_SUCCESS) {
        fwprintf(stderr, L"DDE provider registration failed: %lu\n", (unsigned long)status);
        goto cleanup;
    }
    provider_registered = TRUE;

    client_conversation_id = comtrace_dde_conversation_opened(
        1U,
        TRUE,
        conversation_correlation_id,
        L"ComTraceService",
        L"ComTraceTopic"
    );
    server_conversation_id = comtrace_dde_conversation_opened(
        2U,
        FALSE,
        conversation_correlation_id,
        L"ComTraceService",
        L"ComTraceTopic"
    );
    if (client_conversation_id == 0ULL || server_conversation_id == 0ULL) {
        goto cleanup;
    }

    transaction_instance_id = comtrace_dde_transaction_started(
        client_conversation_id,
        conversation_correlation_id,
        transaction_correlation_id,
        TRUE,
        XTYP_REQUEST,
        XTYP_REQUEST,
        CF_TEXT,
        FALSE,
        L"Item"
    );
    if (transaction_instance_id == 0ULL) {
        goto cleanup;
    }

    comtrace_dde_callback_received(
        server_conversation_id,
        conversation_correlation_id,
        transaction_correlation_id,
        TRUE,
        XTYP_REQUEST,
        CF_TEXT,
        L"item",
        16U
    );

    comtrace_dde_transaction_completed(
        client_conversation_id,
        conversation_correlation_id,
        transaction_instance_id,
        transaction_correlation_id,
        TRUE,
        XTYP_REQUEST,
        DDE_FACK,
        16U,
        TRUE
    );

    comtrace_dde_conversation_closed(client_conversation_id, DDE_FACK);
    comtrace_dde_conversation_closed(server_conversation_id, DDE_FACK);
    result = EXIT_SUCCESS;

cleanup:
    if (provider_registered) {
        comtrace_dde_shutdown();
    }

    if (trace_handle != 0U && properties != NULL) {
        if (!stop_trace_session(trace_handle, properties)) {
            result = EXIT_FAILURE;
        }
    }

    if (properties != NULL) {
        HeapFree(GetProcessHeap(), 0U, properties);
    }

    if (result == EXIT_SUCCESS) {
        wprintf(L"DDE instrumentation fixture PASS\n");
    }

    return result;
}
