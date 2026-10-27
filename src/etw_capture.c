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
    BOOL provider_enabled
)
{
    ULONG status;

    if (provider_enabled) {
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
                L"Warning: provider disable returned status %lu.\n",
                (unsigned long)status
            );
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
    BOOL provider_enabled = FALSE;
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
    properties->LogFileMode = EVENT_TRACE_FILE_MODE_SEQUENTIAL;
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

    if (status != ERROR_SUCCESS) {
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

    provider_enabled = TRUE;

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

    wprintf(L"ComTrace 0.1.0\n");
    wprintf(L"ETW capture started.\n");
    wprintf(L"Provider: Microsoft-Windows-COM-Perf\n");
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
            provider_enabled
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