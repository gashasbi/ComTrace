#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <evntrace.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "etw_capture.h"

static const WCHAR trace_session_name[] = L"ComTrace COM Perf Capture";

static const GUID trace_session_guid = {
    0x5f62e430,
    0x73e9,
    0x4b6a,
    {0x94, 0x9d, 0x1d, 0x2a, 0xcf, 0x2e, 0x50, 0x41}
};

static const GUID com_perf_provider_guid = {
    0xb8d6861b,
    0xd20f,
    0x4eec,
    {0xbb, 0xae, 0x87, 0xe0, 0xdd, 0x80, 0x60, 0x2b}
};

static const GUID ole_perf_provider_guid = {
    0x84958368,
    0x7da7,
    0x49a0,
    {0xb3, 0x3d, 0x07, 0xfa, 0xbb, 0x87, 0x96, 0x26}
};

static const GUID com_rundown_provider_guid = {
    0x2957313d,
    0xfcaa,
    0x5d4a,
    {0x2f, 0x69, 0x32, 0xce, 0x5f, 0x0a, 0xc4, 0x4e}
};

static const GUID com_runtime_provider_guid = {
    0xbf406804,
    0x6afa,
    0x46e7,
    {0x8a, 0x48, 0x6c, 0x35, 0x7e, 0x1d, 0x6d, 0x61}
};

static const GUID rpc_provider_guid = {
    0x6ad52b32,
    0xd609,
    0x4be9,
    {0xae, 0x07, 0xce, 0x8d, 0xae, 0x93, 0x7e, 0x39}
};

static const GUID rpcss_provider_guid = {
    0xd8975f88,
    0x7ddb,
    0x4ed0,
    {0x91, 0xbf, 0x3a, 0xdf, 0x48, 0xc4, 0x8e, 0x0c}
};

static const GUID com_lifetime_provider_guid = {
    0x6f3b5e21,
    0x7c14,
    0x4a90,
    {0x9d, 0x42, 0x81, 0x31, 0x6c, 0xe2, 0xa7, 0x50}
};

static const ULONGLONG ole_perf_keyword = 0x8000000000000000ULL;
static const ULONGLONG com_rundown_keyword = 0x1ULL;
static const ULONGLONG com_runtime_keyword = 0x8ULL;
static const ULONGLONG rpcss_keyword = 0x600ULL;
static const ULONGLONG com_lifetime_keyword = 0x1ULL;

static HANDLE g_stop_event = NULL;

static BOOL WINAPI comtrace_console_handler(DWORD control_type)
{
    if ((control_type == CTRL_C_EVENT ||
         control_type == CTRL_BREAK_EVENT) &&
        g_stop_event != NULL) {
        if (SetEvent(g_stop_event)) {
            return TRUE;
        }
    }

    return FALSE;
}

static int parse_duration(const wchar_t *text, ULONG *duration_seconds)
{
    const wchar_t *cursor;
    wchar_t *end = NULL;
    unsigned long parsed;

    if (text == NULL || text[0] == L'\0' || duration_seconds == NULL) {
        return 0;
    }

    for (cursor = text; *cursor != L'\0'; ++cursor) {
        if (*cursor < L'0' || *cursor > L'9') {
            return 0;
        }
    }

    errno = 0;
    parsed = wcstoul(text, &end, 10);

    if (errno == ERANGE ||
        end == text ||
        end == NULL ||
        *end != L'\0' ||
        parsed < 1UL ||
        parsed > 3600UL) {
        return 0;
    }

    *duration_seconds = (ULONG)parsed;
    return 1;
}

static ULONG stop_trace_session(
    TRACEHANDLE trace_handle,
    PEVENT_TRACE_PROPERTIES properties,
    BOOL com_provider_enabled,
    BOOL ole_provider_enabled,
    BOOL rundown_provider_enabled,
    BOOL com_runtime_provider_enabled,
    BOOL rpc_provider_enabled,
    BOOL rpcss_provider_enabled,
    BOOL com_lifetime_provider_enabled
)
{
    ULONG status;

    if (com_provider_enabled) {
        status = EnableTraceEx2(
            trace_handle,
            &com_perf_provider_guid,
            EVENT_CONTROL_CODE_DISABLE_PROVIDER,
            TRACE_LEVEL_NONE,
            0U,
            0U,
            0U,
            NULL
        );

        if (status != ERROR_SUCCESS) {
            fwprintf(
                stderr,
                L"Warning: COM-Perf provider disable returned status %lu.\n",
                (unsigned long)status
            );
        }
    }

    if (ole_provider_enabled) {
        status = EnableTraceEx2(
            trace_handle,
            &ole_perf_provider_guid,
            EVENT_CONTROL_CODE_DISABLE_PROVIDER,
            TRACE_LEVEL_NONE,
            0U,
            0U,
            0U,
            NULL
        );

        if (status != ERROR_SUCCESS) {
            fwprintf(
                stderr,
                L"Warning: OLE-Perf provider disable returned status %lu.\n",
                (unsigned long)status
            );
        }
    }

    if (rundown_provider_enabled) {
        status = EnableTraceEx2(
            trace_handle,
            &com_rundown_provider_guid,
            EVENT_CONTROL_CODE_DISABLE_PROVIDER,
            TRACE_LEVEL_NONE,
            0U,
            0U,
            0U,
            NULL
        );

        if (status != ERROR_SUCCESS) {
            fwprintf(
                stderr,
                L"Warning: COM-RundownInstrumentation provider disable returned status %lu.\n",
                (unsigned long)status
            );
        }
    }


    if (com_runtime_provider_enabled) {
        status = EnableTraceEx2(
            trace_handle,
            &com_runtime_provider_guid,
            EVENT_CONTROL_CODE_DISABLE_PROVIDER,
            TRACE_LEVEL_NONE,
            0U,
            0U,
            0U,
            NULL
        );

        if (status != ERROR_SUCCESS) {
            fwprintf(stderr, L"Warning: COMRuntime provider disable returned status %lu.\n",
                (unsigned long)status);
        }
    }

    if (rpc_provider_enabled) {
        status = EnableTraceEx2(
            trace_handle,
            &rpc_provider_guid,
            EVENT_CONTROL_CODE_DISABLE_PROVIDER,
            TRACE_LEVEL_NONE,
            0U,
            0U,
            0U,
            NULL
        );

        if (status != ERROR_SUCCESS) {
            fwprintf(stderr, L"Warning: RPC provider disable returned status %lu.\n",
                (unsigned long)status);
        }
    }

    if (rpcss_provider_enabled) {
        status = EnableTraceEx2(
            trace_handle,
            &rpcss_provider_guid,
            EVENT_CONTROL_CODE_DISABLE_PROVIDER,
            TRACE_LEVEL_NONE,
            0U,
            0U,
            0U,
            NULL
        );

        if (status != ERROR_SUCCESS) {
            fwprintf(stderr, L"Warning: RPCSS provider disable returned status %lu.\n",
                (unsigned long)status);
        }
    }


    if (com_lifetime_provider_enabled) {
        status = EnableTraceEx2(
            trace_handle,
            &com_lifetime_provider_guid,
            EVENT_CONTROL_CODE_DISABLE_PROVIDER,
            TRACE_LEVEL_NONE,
            0U,
            0U,
            0U,
            NULL
        );

        if (status != ERROR_SUCCESS) {
            fwprintf(stderr, L"Warning: ComTrace lifetime provider disable returned status %lu.\n",
                (unsigned long)status);
        }
    }

    return ControlTraceW(
        trace_handle,
        trace_session_name,
        properties,
        EVENT_TRACE_CONTROL_STOP
    );
}

int comtrace_capture_com_perf(
    const wchar_t *duration_text,
    const wchar_t *output_path
)
{
    int result = EXIT_FAILURE;
    ULONG duration_seconds = 0U;
    DWORD required_path_chars;
    DWORD full_path_chars;
    DWORD file_attributes;
    DWORD file_error;
    DWORD wait_result;
    ULONG status;
    TRACEHANDLE trace_handle = 0U;
    BOOL session_started = FALSE;
    BOOL com_provider_enabled = FALSE;
    BOOL ole_provider_enabled = FALSE;
    BOOL rundown_provider_enabled = FALSE;
    BOOL com_runtime_provider_enabled = FALSE;
    BOOL rpc_provider_enabled = FALSE;
    BOOL rpcss_provider_enabled = FALSE;
    BOOL com_lifetime_provider_enabled = FALSE;
    BOOL process_lifecycle_enabled = FALSE;
    BOOL handler_registered = FALSE;
    HANDLE stop_event = NULL;
    WCHAR *absolute_path = NULL;
    PEVENT_TRACE_PROPERTIES properties = NULL;
    SIZE_T output_path_bytes;
    SIZE_T properties_bytes;
    const wchar_t *extension;

    if (!parse_duration(duration_text, &duration_seconds)) {
        fwprintf(
            stderr,
            L"Invalid capture duration. Use an integer from 1 to 3600 seconds.\n"
        );
        return EXIT_FAILURE;
    }

    if (output_path == NULL || output_path[0] == L'\0') {
        fwprintf(stderr, L"An ETL output path is required.\n");
        return EXIT_FAILURE;
    }

    extension = wcsrchr(output_path, L'.');
    if (extension == NULL || _wcsicmp(extension, L".etl") != 0) {
        fwprintf(stderr, L"Output path must use the .etl extension.\n");
        return EXIT_FAILURE;
    }

    required_path_chars = GetFullPathNameW(output_path, 0U, NULL, NULL);
    if (required_path_chars == 0U || required_path_chars > 32767U) {
        fwprintf(
            stderr,
            L"Could not resolve the output path. Win32 error: %lu\n",
            (unsigned long)GetLastError()
        );
        goto cleanup;
    }

    absolute_path = (WCHAR *)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        ((SIZE_T)required_path_chars + 1U) * sizeof(WCHAR)
    );

    if (absolute_path == NULL) {
        fwprintf(stderr, L"Unable to allocate output path buffer.\n");
        goto cleanup;
    }

    full_path_chars = GetFullPathNameW(
        output_path,
        required_path_chars + 1U,
        absolute_path,
        NULL
    );

    if (full_path_chars == 0U ||
        full_path_chars >= required_path_chars + 1U) {
        fwprintf(
            stderr,
            L"Could not resolve the output path. Win32 error: %lu\n",
            (unsigned long)GetLastError()
        );
        goto cleanup;
    }

    if (wcslen(absolute_path) >= 1024U) {
        fwprintf(stderr, L"ETW output paths must be shorter than 1024 characters.\n");
        goto cleanup;
    }

    file_attributes = GetFileAttributesW(absolute_path);
    if (file_attributes != INVALID_FILE_ATTRIBUTES) {
        fwprintf(stderr, L"Output file already exists: %ls\n", absolute_path);
        goto cleanup;
    }

    file_error = GetLastError();
    if (file_error != ERROR_FILE_NOT_FOUND) {
        fwprintf(
            stderr,
            L"Could not inspect output path. Win32 error: %lu\n",
            (unsigned long)file_error
        );
        goto cleanup;
    }

    output_path_bytes = (wcslen(absolute_path) + 1U) * sizeof(WCHAR);
    properties_bytes =
        sizeof(EVENT_TRACE_PROPERTIES) +
        sizeof(trace_session_name) +
        output_path_bytes;

    if (properties_bytes > (SIZE_T)ULONG_MAX) {
        fwprintf(stderr, L"ETW session properties are too large.\n");
        goto cleanup;
    }

    properties = (PEVENT_TRACE_PROPERTIES)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        properties_bytes
    );

    if (properties == NULL) {
        fwprintf(stderr, L"Unable to allocate ETW session properties.\n");
        goto cleanup;
    }

    properties->Wnode.BufferSize = (ULONG)properties_bytes;
    properties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    properties->Wnode.Guid = trace_session_guid;
    properties->Wnode.ClientContext = 1U;
    properties->BufferSize = 64U;
    properties->MinimumBuffers = 4U;
    properties->MaximumBuffers = 16U;
    properties->MaximumFileSize = 64U;
    properties->LogFileMode =
        EVENT_TRACE_FILE_MODE_SEQUENTIAL | EVENT_TRACE_SYSTEM_LOGGER_MODE;
    properties->EnableFlags = EVENT_TRACE_FLAG_PROCESS;
    properties->FlushTimer = 1U;
    properties->LoggerNameOffset = (ULONG)sizeof(EVENT_TRACE_PROPERTIES);
    properties->LogFileNameOffset = (ULONG)(
        sizeof(EVENT_TRACE_PROPERTIES) + sizeof(trace_session_name)
    );

    memcpy(
        (BYTE *)properties + properties->LogFileNameOffset,
        absolute_path,
        output_path_bytes
    );

    status = StartTraceW(
        &trace_handle,
        trace_session_name,
        properties
    );

    if (status == ERROR_SUCCESS) {
        process_lifecycle_enabled = TRUE;
    } else {
        ULONG system_logger_status = status;

        properties->LogFileMode = EVENT_TRACE_FILE_MODE_SEQUENTIAL;
        properties->EnableFlags = 0U;
        status = StartTraceW(
            &trace_handle,
            trace_session_name,
            properties
        );

        if (status == ERROR_SUCCESS) {
            fwprintf(
                stderr,
                L"Warning: Process start/stop events are unavailable. System logger start returned status %lu.\n",
                (unsigned long)system_logger_status
            );
        } else {
            fwprintf(
                stderr,
                L"StartTraceW failed with status %lu.\n",
                (unsigned long)status
            );

            if (status == ERROR_ACCESS_DENIED) {
                fwprintf(
                    stderr,
                    L"Retry from an elevated Developer PowerShell if appropriate.\n"
                );
            }

            goto cleanup;
        }
    }

    session_started = TRUE;

    status = EnableTraceEx2(
        trace_handle,
        &com_perf_provider_guid,
        EVENT_CONTROL_CODE_ENABLE_PROVIDER,
        TRACE_LEVEL_VERBOSE,
        0U,
        0U,
        0U,
        NULL
    );

    if (status != ERROR_SUCCESS) {
        fwprintf(
            stderr,
            L"EnableTraceEx2 failed with status %lu.\n",
            (unsigned long)status
        );

        if (status == ERROR_ACCESS_DENIED) {
            fwprintf(
                stderr,
                L"Retry from an elevated Developer PowerShell if appropriate.\n"
            );
        }

        goto cleanup;
    }

    com_provider_enabled = TRUE;

    status = EnableTraceEx2(
        trace_handle,
        &ole_perf_provider_guid,
        EVENT_CONTROL_CODE_ENABLE_PROVIDER,
        TRACE_LEVEL_VERBOSE,
        ole_perf_keyword,
        0U,
        0U,
        NULL
    );

    if (status != ERROR_SUCCESS) {
        fwprintf(
            stderr,
            L"EnableTraceEx2 for OLE-Perf failed with status %lu.\n",
            (unsigned long)status
        );

        if (status == ERROR_ACCESS_DENIED) {
            fwprintf(
                stderr,
                L"Retry from an elevated Developer PowerShell if appropriate.\n"
            );
        }

        goto cleanup;
    }

    ole_provider_enabled = TRUE;

    status = EnableTraceEx2(
        trace_handle,
        &com_rundown_provider_guid,
        EVENT_CONTROL_CODE_ENABLE_PROVIDER,
        TRACE_LEVEL_VERBOSE,
        com_rundown_keyword,
        0U,
        0U,
        NULL
    );

    if (status == ERROR_SUCCESS) {
        rundown_provider_enabled = TRUE;
    } else {
        fwprintf(
            stderr,
            L"Warning: COM-RundownInstrumentation provider enable returned status %lu.\n",
            (unsigned long)status
        );
    }


    status = EnableTraceEx2(
        trace_handle,
        &com_runtime_provider_guid,
        EVENT_CONTROL_CODE_ENABLE_PROVIDER,
        TRACE_LEVEL_INFORMATION,
        com_runtime_keyword,
        0U,
        0U,
        NULL
    );

    if (status == ERROR_SUCCESS) {
        com_runtime_provider_enabled = TRUE;
    } else {
        fwprintf(stderr, L"Warning: COMRuntime provider enable returned status %lu.\n",
            (unsigned long)status);
    }

    status = EnableTraceEx2(
        trace_handle,
        &rpc_provider_guid,
        EVENT_CONTROL_CODE_ENABLE_PROVIDER,
        TRACE_LEVEL_INFORMATION,
        0U,
        0U,
        0U,
        NULL
    );

    if (status == ERROR_SUCCESS) {
        rpc_provider_enabled = TRUE;
    } else {
        fwprintf(stderr, L"Warning: RPC provider enable returned status %lu.\n",
            (unsigned long)status);
    }

    status = EnableTraceEx2(
        trace_handle,
        &rpcss_provider_guid,
        EVENT_CONTROL_CODE_ENABLE_PROVIDER,
        TRACE_LEVEL_INFORMATION,
        rpcss_keyword,
        0U,
        0U,
        NULL
    );

    if (status == ERROR_SUCCESS) {
        rpcss_provider_enabled = TRUE;
    } else {
        fwprintf(stderr, L"Warning: RPCSS provider enable returned status %lu.\n",
            (unsigned long)status);
    }


    status = EnableTraceEx2(
        trace_handle,
        &com_lifetime_provider_guid,
        EVENT_CONTROL_CODE_ENABLE_PROVIDER,
        TRACE_LEVEL_INFORMATION,
        com_lifetime_keyword,
        0U,
        0U,
        NULL
    );

    if (status == ERROR_SUCCESS) {
        com_lifetime_provider_enabled = TRUE;
    } else {
        fwprintf(stderr, L"Warning: ComTrace lifetime provider enable returned status %lu.\n",
            (unsigned long)status);
    }

    stop_event = CreateEventW(NULL, TRUE, FALSE, NULL);
    if (stop_event == NULL) {
        fwprintf(
            stderr,
            L"CreateEventW failed with Win32 error %lu.\n",
            (unsigned long)GetLastError()
        );
        goto cleanup;
    }

    g_stop_event = stop_event;

    if (!SetConsoleCtrlHandler(comtrace_console_handler, TRUE)) {
        g_stop_event = NULL;
        fwprintf(
            stderr,
            L"Could not register the console control handler. Win32 error: %lu\n",
            (unsigned long)GetLastError()
        );
        goto cleanup;
    }

    handler_registered = TRUE;

    wprintf(L"ComTrace 0.2.0\n");
    wprintf(L"ETW capture started.\n");
    wprintf(L"Provider: Microsoft-Windows-COM-Perf\n");
    wprintf(L"Provider: Microsoft-Windows-OLE-Perf\n");
    if (rundown_provider_enabled) {
        wprintf(L"Provider: Microsoft-Windows-COM-RundownInstrumentation\n");
    }
    if (com_runtime_provider_enabled) {
        wprintf(L"Provider: Microsoft-Windows-COMRuntime (PrematureStubRundown)\n");
    }
    if (rpc_provider_enabled) {
        wprintf(L"Provider: Microsoft-Windows-RPC\n");
    }
    if (rpcss_provider_enabled) {
        wprintf(L"Provider: Microsoft-Windows-RPCSS (interface registration)\n");
    }
    if (com_lifetime_provider_enabled) {
        wprintf(L"Provider: ComTrace.COMLifetime (application instrumentation)\n");
    }
    if (process_lifecycle_enabled) {
        wprintf(L"Provider: Kernel Process start/stop events\n");
    }
    wprintf(L"Output: %ls\n", absolute_path);
    wprintf(L"Duration: %lu seconds\n", (unsigned long)duration_seconds);
    wprintf(L"Reproduce the behavior now. Press Ctrl+C to stop early.\n");
    (void)fflush(stdout);

    wait_result = WaitForSingleObject(
        stop_event,
        duration_seconds * 1000UL
    );

    if (wait_result == WAIT_TIMEOUT) {
        result = EXIT_SUCCESS;
        wprintf(L"Capture duration completed.\n");
    } else if (wait_result == WAIT_OBJECT_0) {
        result = EXIT_SUCCESS;
        wprintf(L"Capture stopped by user request.\n");
    } else {
        result = EXIT_FAILURE;
        fwprintf(
            stderr,
            L"WaitForSingleObject failed with Win32 error %lu.\n",
            (unsigned long)GetLastError()
        );
    }

cleanup:
    if (handler_registered) {
        (void)SetConsoleCtrlHandler(comtrace_console_handler, FALSE);
    }

    g_stop_event = NULL;

    if (stop_event != NULL) {
        (void)CloseHandle(stop_event);
    }

    if (session_started) {
        status = stop_trace_session(
            trace_handle,
            properties,
            com_provider_enabled,
            ole_provider_enabled,
            rundown_provider_enabled,
            com_runtime_provider_enabled,
            rpc_provider_enabled,
            rpcss_provider_enabled,
            com_lifetime_provider_enabled
        );

        if (status != ERROR_SUCCESS) {
            fwprintf(
                stderr,
                L"ControlTraceW failed with status %lu.\n",
                (unsigned long)status
            );
            result = EXIT_FAILURE;
        } else {
            wprintf(
                L"Trace finalized. Buffers written: %lu; events lost: %lu; log buffers lost: %lu.\n",
                (unsigned long)properties->BuffersWritten,
                (unsigned long)properties->EventsLost,
                (unsigned long)properties->LogBuffersLost
            );
            wprintf(L"ETL file: %ls\n", absolute_path);
        }
    }

    if (properties != NULL) {
        (void)HeapFree(GetProcessHeap(), 0U, properties);
    }

    if (absolute_path != NULL) {
        (void)HeapFree(GetProcessHeap(), 0U, absolute_path);
    }

    return result;
}