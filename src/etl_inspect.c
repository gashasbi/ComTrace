#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <evntrace.h>
#include <evntcons.h>
#include <tdh.h>
#include <stddef.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "etl_inspect.h"

#define MAX_EVENT_TYPES 512UL
#define MAX_SAMPLE_EVENTS 10000UL

typedef struct {
    GUID provider_id;
    USHORT event_id;
    UCHAR version;
    UCHAR opcode;
    ULONGLONG count;
} ETL_EVENT_KIND;

typedef struct {
    ULONGLONG total_events;
    ULONGLONG untracked_event_type_events;
    ULONG sample_limit;
    ULONG sampled_events;
    ULONG kind_count;
    ETL_EVENT_KIND kinds[MAX_EVENT_TYPES];
} ETL_INSPECT_CONTEXT;

static void print_guid(const GUID *guid)
{
    wprintf(
        L"{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
        (unsigned long)guid->Data1,
        (unsigned int)guid->Data2,
        (unsigned int)guid->Data3,
        (unsigned int)guid->Data4[0],
        (unsigned int)guid->Data4[1],
        (unsigned int)guid->Data4[2],
        (unsigned int)guid->Data4[3],
        (unsigned int)guid->Data4[4],
        (unsigned int)guid->Data4[5],
        (unsigned int)guid->Data4[6],
        (unsigned int)guid->Data4[7]
    );
}

static int parse_sample_limit(const wchar_t *text, ULONG *limit)
{
    const wchar_t *cursor;
    wchar_t *end = NULL;
    unsigned long parsed;

    if (text == NULL || text[0] == L'\0' || limit == NULL) {
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
        parsed > MAX_SAMPLE_EVENTS) {
        return 0;
    }

    *limit = (ULONG)parsed;
    return 1;
}

static void print_timestamp(const EVENT_RECORD *event_record)
{
    FILETIME file_time;
    SYSTEMTIME system_time;
    ULONGLONG ticks;

    ticks = (ULONGLONG)event_record->EventHeader.TimeStamp.QuadPart;
    file_time.dwLowDateTime = (DWORD)(ticks & 0xffffffffULL);
    file_time.dwHighDateTime = (DWORD)(ticks >> 32U);

    if (FileTimeToSystemTime(&file_time, &system_time)) {
        wprintf(
            L"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",
            (unsigned int)system_time.wYear,
            (unsigned int)system_time.wMonth,
            (unsigned int)system_time.wDay,
            (unsigned int)system_time.wHour,
            (unsigned int)system_time.wMinute,
            (unsigned int)system_time.wSecond,
            (unsigned int)system_time.wMilliseconds
        );
    } else {
        wprintf(L"(timestamp unavailable)");
    }
}

static void record_event_kind(
    ETL_INSPECT_CONTEXT *context,
    const EVENT_RECORD *event_record
)
{
    ULONG index;

    for (index = 0U; index < context->kind_count; ++index) {
        ETL_EVENT_KIND *kind = &context->kinds[index];

        if (memcmp(
                &kind->provider_id,
                &event_record->EventHeader.ProviderId,
                sizeof(GUID)
            ) == 0 &&
            kind->event_id == event_record->EventHeader.EventDescriptor.Id &&
            kind->version == event_record->EventHeader.EventDescriptor.Version &&
            kind->opcode == event_record->EventHeader.EventDescriptor.Opcode) {
            ++kind->count;
            return;
        }
    }

    if (context->kind_count >= MAX_EVENT_TYPES) {
        ++context->untracked_event_type_events;
        return;
    }

    {
        ETL_EVENT_KIND *kind = &context->kinds[context->kind_count];

        kind->provider_id = event_record->EventHeader.ProviderId;
        kind->event_id = event_record->EventHeader.EventDescriptor.Id;
        kind->version = event_record->EventHeader.EventDescriptor.Version;
        kind->opcode = event_record->EventHeader.EventDescriptor.Opcode;
        kind->count = 1U;
        ++context->kind_count;
    }
}

static void print_event_properties(const EVENT_RECORD *event_record)
{
    ULONG buffer_size = 0U;
    ULONG status;
    ULONG available_count;
    ULONG property_count;
    ULONG index;
    BYTE *buffer;
    TRACE_EVENT_INFO *event_info;
    size_t property_offset = offsetof(TRACE_EVENT_INFO, EventPropertyInfoArray);

    status = TdhGetEventInformation(
        (PEVENT_RECORD)event_record,
        0U,
        NULL,
        NULL,
        &buffer_size
    );

    if (status != ERROR_INSUFFICIENT_BUFFER ||
        buffer_size < property_offset ||
        buffer_size > 65536U) {
        wprintf(L"  payload_schema=unavailable status=%lu\n", (unsigned long)status);
        return;
    }

    buffer = (BYTE *)malloc(buffer_size);
    if (buffer == NULL) {
        wprintf(L"  payload_schema=unavailable reason=allocation_failed\n");
        return;
    }

    status = TdhGetEventInformation(
        (PEVENT_RECORD)event_record,
        0U,
        NULL,
        (PTRACE_EVENT_INFO)buffer,
        &buffer_size
    );

    if (status != ERROR_SUCCESS || buffer_size < property_offset) {
        wprintf(L"  payload_schema=unavailable status=%lu\n", (unsigned long)status);
        free(buffer);
        return;
    }

    event_info = (TRACE_EVENT_INFO *)buffer;
    available_count = (ULONG)((buffer_size - property_offset) / sizeof(EVENT_PROPERTY_INFO));
    property_count = event_info->TopLevelPropertyCount;

    if (property_count > available_count) {
        property_count = available_count;
    }
    if (property_count > 64U) {
        property_count = 64U;
    }

    wprintf(L"  payload_properties=%lu\n", (unsigned long)property_count);

    for (index = 0U; index < property_count; ++index) {
        const EVENT_PROPERTY_INFO *property =
            &event_info->EventPropertyInfoArray[index];
        ULONG name_offset = property->NameOffset;
        ULONG remaining;
        ULONG name_length = 0U;
        const wchar_t *name;

        if (name_offset >= buffer_size ||
            (name_offset % (ULONG)sizeof(wchar_t)) != 0U) {
            wprintf(L"  property[%lu]=invalid_name_offset\n", (unsigned long)index);
            continue;
        }

        name = (const wchar_t *)(buffer + name_offset);
        remaining = (buffer_size - name_offset) / (ULONG)sizeof(wchar_t);

        while (name_length < remaining && name[name_length] != L'\0') {
            ++name_length;
        }

        if (name_length == remaining) {
            wprintf(L"  property[%lu]=unterminated_name\n", (unsigned long)index);
            continue;
        }

        if ((property->Flags & PropertyStruct) != 0U) {
            wprintf(
                L"  property[%lu] name=%.*ls type=struct\n",
                (unsigned long)index,
                (int)name_length,
                name
            );
        } else {
            wprintf(
                L"  property[%lu] name=%.*ls in_type=%u out_type=%u\n",
                (unsigned long)index,
                (int)name_length,
                name,
                (unsigned int)property->nonStructType.InType,
                (unsigned int)property->nonStructType.OutType
            );
        }
    }

    free(buffer);
}

static VOID WINAPI inspect_event_callback(PEVENT_RECORD event_record)
{
    ETL_INSPECT_CONTEXT *context;

    if (event_record == NULL || event_record->UserContext == NULL) {
        return;
    }

    context = (ETL_INSPECT_CONTEXT *)event_record->UserContext;
    ++context->total_events;
    record_event_kind(context, event_record);

    if (context->sampled_events >= context->sample_limit) {
        return;
    }

    ++context->sampled_events;

    wprintf(
        L"sample=%lu time=",
        (unsigned long)context->sampled_events
    );
    print_timestamp(event_record);

    wprintf(L" provider=");
    print_guid(&event_record->EventHeader.ProviderId);

    wprintf(
        L" event_id=%u version=%u opcode=%u pid=%lu tid=%lu\n",
        (unsigned int)event_record->EventHeader.EventDescriptor.Id,
        (unsigned int)event_record->EventHeader.EventDescriptor.Version,
        (unsigned int)event_record->EventHeader.EventDescriptor.Opcode,
        (unsigned long)event_record->EventHeader.ProcessId,
        (unsigned long)event_record->EventHeader.ThreadId
    );
    print_event_properties(event_record);
}

int comtrace_inspect_etl(
    const wchar_t *etl_path,
    const wchar_t *limit_text
)
{
    ETL_INSPECT_CONTEXT context;
    EVENT_TRACE_LOGFILEW logfile;
    TRACEHANDLE trace_handle;
    ULONG process_status;
    ULONG close_status;
    ULONG index;
    DWORD attributes;
    int result = EXIT_FAILURE;

    if (etl_path == NULL || etl_path[0] == L'\0') {
        fwprintf(stderr, L"An ETL input path is required.\n");
        return EXIT_FAILURE;
    }

    ZeroMemory(&context, sizeof(context));
    ZeroMemory(&logfile, sizeof(logfile));

    if (!parse_sample_limit(limit_text, &context.sample_limit)) {
        fwprintf(
            stderr,
            L"Invalid max-events value. Use an integer from 1 to 10000.\n"
        );
        return EXIT_FAILURE;
    }

    attributes = GetFileAttributesW(etl_path);
    if (attributes == INVALID_FILE_ATTRIBUTES) {
        DWORD error_code = GetLastError();
        fwprintf(
            stderr,
            L"Unable to access ETL input path. Win32 error: %lu\n",
            (unsigned long)error_code
        );
        return EXIT_FAILURE;
    }

    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0U) {
        fwprintf(stderr, L"ETL input path is a directory.\n");
        return EXIT_FAILURE;
    }

    logfile.LogFileName = (LPWSTR)etl_path;
    logfile.ProcessTraceMode = PROCESS_TRACE_MODE_EVENT_RECORD;
    logfile.EventRecordCallback = inspect_event_callback;
    logfile.Context = &context;

    trace_handle = OpenTraceW(&logfile);

    if (trace_handle == INVALID_PROCESSTRACE_HANDLE) {
        fwprintf(
            stderr,
            L"OpenTraceW failed with Win32 error %lu.\n",
            (unsigned long)GetLastError()
        );
        return EXIT_FAILURE;
    }

    wprintf(L"ComTrace 0.1.0\n");
    wprintf(L"ETL inspection\n");
    wprintf(L"Input: %ls\n", etl_path);
    wprintf(L"Sample limit: %lu\n\n", (unsigned long)context.sample_limit);
    wprintf(L"Event samples:\n");

    process_status = ProcessTrace(&trace_handle, 1U, NULL, NULL);
    close_status = CloseTrace(trace_handle);

    wprintf(L"\nEvent summary\n");
    wprintf(
        L"Events processed: %llu\n",
        (unsigned long long)context.total_events
    );
    wprintf(
        L"Event samples printed: %lu\n",
        (unsigned long)context.sampled_events
    );
    wprintf(
        L"Distinct tracked event types: %lu\n",
        (unsigned long)context.kind_count
    );
    wprintf(
        L"Events in untracked event types: %llu\n",
        (unsigned long long)context.untracked_event_type_events
    );

    wprintf(L"\nProvider GUID | Event ID | Version | Opcode | Count\n");

    for (index = 0U; index < context.kind_count; ++index) {
        const ETL_EVENT_KIND *kind = &context.kinds[index];

        print_guid(&kind->provider_id);
        wprintf(
            L" | %u | %u | %u | %llu\n",
            (unsigned int)kind->event_id,
            (unsigned int)kind->version,
            (unsigned int)kind->opcode,
            (unsigned long long)kind->count
        );
    }

    if (process_status != ERROR_SUCCESS) {
        fwprintf(
            stderr,
            L"ProcessTrace failed with status %lu.\n",
            (unsigned long)process_status
        );
        return EXIT_FAILURE;
    }

    if (close_status != ERROR_SUCCESS) {
        fwprintf(
            stderr,
            L"CloseTrace failed with status %lu.\n",
            (unsigned long)close_status
        );
        return EXIT_FAILURE;
    }

    result = EXIT_SUCCESS;
    return result;
}