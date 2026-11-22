#include <windows.h>
#include <evntrace.h>
#include <unknwn.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include "comtrace_lifetime.h"

static const WCHAR trace_session_name[] = L"ComTrace Lifetime Fixture";

static const GUID trace_session_guid = {
    0x0c97f68a,
    0x2b14,
    0x4c51,
    {0x9f, 0x73, 0x6d, 0x2a, 0x8b, 0xe1, 0x40, 0x55}
};

static const GUID lifetime_provider_guid = {
    0x6f3b5e21,
    0x7c14,
    0x4a90,
    {0x9d, 0x42, 0x81, 0x31, 0x6c, 0xe2, 0xa7, 0x50}
};

static const GUID fixture_class_id = {
    0x183a5c71,
    0x2f49,
    0x4d83,
    {0x91, 0x06, 0x27, 0x4a, 0x5b, 0x6c, 0x7d, 0x8e}
};

static const GUID fixture_interface_id = {
    0x296b7d82,
    0x3a5c,
    0x4e94,
    {0xa2, 0x17, 0x38, 0x5b, 0x6c, 0x7d, 0x8e, 0x9f}
};

typedef struct {
    IUnknown interface_pointer;
    volatile LONG reference_count;
    ULONGLONG object_instance_id;
} COMTRACE_FIXTURE_OBJECT;

static HRESULT STDMETHODCALLTYPE fixture_query_interface(
    IUnknown *interface_pointer,
    REFIID interface_id,
    void **object
)
{
    (void)interface_pointer;
    (void)interface_id;

    if (object == NULL) {
        return E_POINTER;
    }

    *object = NULL;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE fixture_add_ref(IUnknown *interface_pointer)
{
    COMTRACE_FIXTURE_OBJECT *object =
        (COMTRACE_FIXTURE_OBJECT *)interface_pointer;
    LONG reference_count = InterlockedIncrement(&object->reference_count);

    comtrace_lifetime_add_ref(
        object->object_instance_id,
        &fixture_interface_id,
        (ULONG)reference_count
    );

    return (ULONG)reference_count;
}

static ULONG STDMETHODCALLTYPE fixture_release(IUnknown *interface_pointer)
{
    COMTRACE_FIXTURE_OBJECT *object =
        (COMTRACE_FIXTURE_OBJECT *)interface_pointer;
    LONG reference_count = InterlockedDecrement(&object->reference_count);
    ULONGLONG object_instance_id = object->object_instance_id;

    comtrace_lifetime_release(
        object_instance_id,
        &fixture_interface_id,
        (ULONG)reference_count
    );

    if (reference_count == 0) {
        comtrace_lifetime_object_destroyed(
            object_instance_id,
            &fixture_interface_id
        );
        (void)HeapFree(GetProcessHeap(), 0U, object);
    }

    return (ULONG)reference_count;
}

static IUnknownVtbl fixture_vtable = {
    fixture_query_interface,
    fixture_add_ref,
    fixture_release
};

static int start_trace_session(
    const wchar_t *etl_path,
    TRACEHANDLE *trace_handle,
    PEVENT_TRACE_PROPERTIES *properties_out
)
{
    SIZE_T logger_chars = sizeof(trace_session_name) / sizeof(WCHAR);
    SIZE_T path_chars = wcslen(etl_path) + 1U;
    SIZE_T properties_size = sizeof(EVENT_TRACE_PROPERTIES) +
        (logger_chars + path_chars) * sizeof(WCHAR);
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
        (ULONG)(logger_chars * sizeof(WCHAR));

    CopyMemory(
        (BYTE *)properties + properties->LoggerNameOffset,
        trace_session_name,
        logger_chars * sizeof(WCHAR)
    );
    CopyMemory(
        (BYTE *)properties + properties->LogFileNameOffset,
        etl_path,
        path_chars * sizeof(WCHAR)
    );

    status = StartTraceW(trace_handle, trace_session_name, properties);
    if (status != ERROR_SUCCESS) {
        fwprintf(stderr, L"StartTraceW failed: %lu\n", (unsigned long)status);
        (void)HeapFree(GetProcessHeap(), 0U, properties);
        return 0;
    }

    status = EnableTraceEx2(
        *trace_handle,
        &lifetime_provider_guid,
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
        (void)HeapFree(GetProcessHeap(), 0U, properties);
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
    ULONG add_ref_count;
    ULONG release_count;
    TRACEHANDLE trace_handle = 0U;
    PEVENT_TRACE_PROPERTIES properties = NULL;
    COMTRACE_FIXTURE_OBJECT *object = NULL;
    BOOL provider_registered = FALSE;

    if (argc != 2 || argv[1] == NULL || argv[1][0] == L'\0') {
        fwprintf(stderr, L"Usage: ComTraceComLifetimeFixture <output.etl>\n");
        return EXIT_FAILURE;
    }

    if (!start_trace_session(argv[1], &trace_handle, &properties)) {
        goto cleanup;
    }

    status = comtrace_lifetime_initialize();
    if (status != ERROR_SUCCESS) {
        fwprintf(stderr, L"TraceLogging registration failed: %lu\n",
            (unsigned long)status);
        goto cleanup;
    }
    provider_registered = TRUE;

    object = (COMTRACE_FIXTURE_OBJECT *)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        sizeof(*object)
    );
    if (object == NULL) {
        goto cleanup;
    }

    object->interface_pointer.lpVtbl = &fixture_vtable;
    object->reference_count = 1;
    object->object_instance_id = comtrace_lifetime_object_created(
        &fixture_class_id,
        &fixture_interface_id,
        1U
    );
    if (object->object_instance_id == 0ULL) {
        goto cleanup;
    }

    add_ref_count = object->interface_pointer.lpVtbl->AddRef(
        &object->interface_pointer
    );
    if (add_ref_count != 2U) {
        goto cleanup;
    }

    release_count = object->interface_pointer.lpVtbl->Release(
        &object->interface_pointer
    );
    if (release_count != 1U) {
        goto cleanup;
    }

    release_count = object->interface_pointer.lpVtbl->Release(
        &object->interface_pointer
    );
    object = NULL;
    if (release_count != 0U) {
        goto cleanup;
    }

    result = EXIT_SUCCESS;

cleanup:
    if (object != NULL) {
        while (InterlockedCompareExchange(&object->reference_count, 0, 0) > 0) {
            (void)object->interface_pointer.lpVtbl->Release(
                &object->interface_pointer
            );
        }
    }

    if (provider_registered) {
        comtrace_lifetime_shutdown();
    }

    if (trace_handle != 0U && properties != NULL) {
        if (!stop_trace_session(trace_handle, properties)) {
            result = EXIT_FAILURE;
        }
    }

    if (properties != NULL) {
        (void)HeapFree(GetProcessHeap(), 0U, properties);
    }

    if (result == EXIT_SUCCESS) {
        wprintf(L"COM lifetime fixture PASS\n");
    }

    return result;
}
