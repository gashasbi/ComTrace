#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <evntrace.h>
#include <evntcons.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "com_identity.h"

static const GUID fixture_provider_id = {
    0xd3a24c71, 0x91b6, 0x4b02,
    { 0x8c, 0x37, 0x5a, 0x6d, 0x7e, 0x80, 0x91, 0xaf }
};

static const GUID expected_class_id = {
    0x00112233, 0x4455, 0x6677,
    { 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff }
};

static const GUID expected_interface_id = {
    0x10213243, 0x5465, 0x7687,
    { 0x98, 0xa9, 0xba, 0xcb, 0xdc, 0xed, 0xfe, 0x0f }
};

static const ULONG expected_target_process_id = 0x12345678UL;
static const ULONG expected_target_thread_id = 0x87654321UL;

static const GUID expected_activity_id = {
    0x20314253, 0x6475, 0x8697,
    { 0xa8, 0xb9, 0xca, 0xdb, 0xec, 0xfd, 0x0e, 0x1f }
};

static const GUID expected_related_activity_id = {
    0x30415263, 0x7485, 0x96a7,
    { 0xb8, 0xc9, 0xda, 0xeb, 0xfc, 0x0d, 0x1e, 0x2f }
};

typedef struct {
    ULONG events_seen;
    ULONG failures;
} IDENTITY_TEST_CONTEXT;

static VOID WINAPI identity_event_callback(PEVENT_RECORD event_record)
{
    IDENTITY_TEST_CONTEXT *context;
    GUID value;
    ULONG number = 0U;
    COMTRACE_ACTIVITY_IDS activity_ids;
    COMTRACE_IDENTITY_STATUS status;

    if (event_record == NULL || event_record->UserContext == NULL) {
        return;
    }

    context = (IDENTITY_TEST_CONTEXT *)event_record->UserContext;

    if (memcmp(
            &event_record->EventHeader.ProviderId,
            &fixture_provider_id,
            sizeof(fixture_provider_id)
        ) != 0) {
        return;
    }

    ++context->events_seen;

    status = comtrace_extract_guid_property(event_record, L"ClassId", &value);
    if (status != COMTRACE_IDENTITY_SUCCESS ||
        memcmp(&value, &expected_class_id, sizeof(value)) != 0) {
        fwprintf(stderr, L"ClassId extraction failed: status=%u\n", (unsigned int)status);
        ++context->failures;
    }

    status = comtrace_extract_guid_property(event_record, L"TargetInterface", &value);
    if (status != COMTRACE_IDENTITY_SUCCESS ||
        memcmp(&value, &expected_interface_id, sizeof(value)) != 0) {
        fwprintf(stderr, L"TargetInterface extraction failed: status=%u\n", (unsigned int)status);
        ++context->failures;
    }

    status = comtrace_extract_uint32_property(
        event_record,
        L"TargetProcessId",
        &number
    );
    if (status != COMTRACE_IDENTITY_SUCCESS ||
        number != expected_target_process_id) {
        fwprintf(stderr, L"TargetProcessId extraction failed: status=%u value=%lu\n", (unsigned int)status, (unsigned long)number);
        ++context->failures;
    }

    status = comtrace_extract_uint32_property(
        event_record,
        L"TargetThreadId",
        &number
    );
    if (status != COMTRACE_IDENTITY_SUCCESS ||
        number != expected_target_thread_id) {
        fwprintf(stderr, L"TargetThreadId extraction failed: status=%u value=%lu\n", (unsigned int)status, (unsigned long)number);
        ++context->failures;
    }

    status = comtrace_extract_uint32_property(event_record, L"NotGuid", &number);
    if (status != COMTRACE_IDENTITY_PROPERTY_UNSUPPORTED) {
        fwprintf(stderr, L"NotGuid UINT32 type rejection failed: status=%u\n", (unsigned int)status);
        ++context->failures;
    }

    status = comtrace_extract_guid_property(event_record, L"NotGuid", &value);
    if (status != COMTRACE_IDENTITY_PROPERTY_UNSUPPORTED) {
        fwprintf(stderr, L"NotGuid GUID type rejection failed: status=%u\n", (unsigned int)status);
        ++context->failures;
    }

    status = comtrace_extract_guid_property(event_record, L"MissingGuid", &value);
    if (status != COMTRACE_IDENTITY_PROPERTY_NOT_FOUND) {
        fwprintf(stderr, L"MissingGuid lookup failed: status=%u\n", (unsigned int)status);
        ++context->failures;
    }

    status = comtrace_extract_activity_ids(event_record, &activity_ids);
    if (status != COMTRACE_IDENTITY_SUCCESS ||
        !activity_ids.has_activity_id ||
        !activity_ids.has_related_activity_id ||
        memcmp(&activity_ids.activity_id, &expected_activity_id, sizeof(GUID)) != 0 ||
        memcmp(&activity_ids.related_activity_id, &expected_related_activity_id, sizeof(GUID)) != 0) {
        fwprintf(stderr, L"Activity ID extraction failed: status=%u\n", (unsigned int)status);
        ++context->failures;
    }
}

static int verify_event(const wchar_t *etl_path)
{
    EVENT_TRACE_LOGFILEW logfile;
    TRACEHANDLE trace_handle;
    ULONG process_status;
    ULONG close_status;
    IDENTITY_TEST_CONTEXT context;

    ZeroMemory(&logfile, sizeof(logfile));
    ZeroMemory(&context, sizeof(context));

    logfile.LogFileName = (LPWSTR)etl_path;
    logfile.ProcessTraceMode = PROCESS_TRACE_MODE_EVENT_RECORD;
    logfile.EventRecordCallback = identity_event_callback;
    logfile.Context = &context;

    trace_handle = OpenTraceW(&logfile);
    if (trace_handle == INVALID_PROCESSTRACE_HANDLE) {
        fwprintf(stderr, L"OpenTraceW failed: %lu\n", (unsigned long)GetLastError());
        return 0;
    }

    process_status = ProcessTrace(&trace_handle, 1U, NULL, NULL);
    close_status = CloseTrace(trace_handle);

    if (process_status != ERROR_SUCCESS || close_status != ERROR_SUCCESS) {
        fwprintf(
            stderr,
            L"Trace processing failed. ProcessTrace: %lu CloseTrace: %lu\n",
            (unsigned long)process_status,
            (unsigned long)close_status
        );
        return 0;
    }

    if (context.events_seen != 1U || context.failures != 0U) {
        fwprintf(
            stderr,
            L"Identity validation failed. Events: %lu Failures: %lu\n",
            (unsigned long)context.events_seen,
            (unsigned long)context.failures
        );
        return 0;
    }

    return 1;
}

int wmain(int argc, wchar_t **argv)
{
    GUID value;

    if (argc != 2) {
        fwprintf(stderr, L"Usage: ComTraceComIdentityTests <etl-path>\n");
        return EXIT_FAILURE;
    }

    if (comtrace_extract_guid_property(NULL, L"ClassId", &value) !=
            COMTRACE_IDENTITY_INVALID_ARGUMENT ||
        comtrace_extract_guid_property(NULL, L"ClassId", NULL) !=
            COMTRACE_IDENTITY_INVALID_ARGUMENT) {
        fwprintf(stderr, L"Invalid argument handling failed.\n");
        return EXIT_FAILURE;
    }

    if (!verify_event(argv[1])) {
        return EXIT_FAILURE;
    }

    wprintf(L"COM identity GUID properties were extracted correctly.\n");
    return EXIT_SUCCESS;
}
