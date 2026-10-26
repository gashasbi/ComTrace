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
#include "etl_value.h"

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

static void print_event_property_value(
    const EVENT_RECORD *event_record,
    const EVENT_PROPERTY_INFO *property,
    const wchar_t *name,
    ULONG index)
{
    PROPERTY_DATA_DESCRIPTOR descriptor;
    BYTE value[4096];
    ULONG value_size = 0U;
    ULONG status;
    ULONG flags = (ULONG)property->Flags;
    USHORT in_type = property->nonStructType.InType;

    if ((flags & PropertyStruct) != 0U ||
        (flags & PropertyParamCount) != 0U ||
        (flags & PropertyParamFixedCount) != 0U ||
        (flags & PropertyParamLength) != 0U ||
        (flags & PropertyParamFixedLength) != 0U ||
        (flags & PropertyWBEMXmlFragment) != 0U ||
        (flags & PropertyHasCustomSchema) != 0U) {
        wprintf(
            L"  property_value[%lu] name=%ls status=skipped reason=complex_property\n",
            (unsigned long)index,
            name
        );
        return;
    }

    switch (in_type) {
    case TDH_INTYPE_UNICODESTRING:
    case TDH_INTYPE_ANSISTRING:
    case TDH_INTYPE_INT8:
    case TDH_INTYPE_UINT8:
    case TDH_INTYPE_INT16:
    case TDH_INTYPE_UINT16:
    case TDH_INTYPE_INT32:
    case TDH_INTYPE_UINT32:
    case TDH_INTYPE_INT64:
    case TDH_INTYPE_UINT64:
    case TDH_INTYPE_FLOAT:
    case TDH_INTYPE_DOUBLE:
    case TDH_INTYPE_BOOLEAN:
    case TDH_INTYPE_BINARY:
    case TDH_INTYPE_GUID:
    case TDH_INTYPE_POINTER:
    case TDH_INTYPE_FILETIME:
    case TDH_INTYPE_HEXINT32:
    case TDH_INTYPE_HEXINT64:
    case TDH_INTYPE_SIZET:
    case TDH_INTYPE_HEXDUMP:
        break;
    default:
        wprintf(
            L"  property_value[%lu] name=%ls status=skipped reason=unsupported_type in_type=%u\n",
            (unsigned long)index,
            name,
            (unsigned int)in_type
        );
        return;
    }

    ZeroMemory(&descriptor, sizeof(descriptor));
    descriptor.PropertyName = (ULONGLONG)(ULONG_PTR)name;
    descriptor.ArrayIndex = 0xFFFFFFFFUL;

    status = TdhGetPropertySize(
        (PEVENT_RECORD)event_record,
        0U,
        NULL,
        1U,
        &descriptor,
        &value_size
    );

    if (status != ERROR_SUCCESS) {
        wprintf(
            L"  property_value[%lu] name=%ls status=unavailable reason=size_query_failed tdh_status=%lu\n",
            (unsigned long)index,
            name,
            (unsigned long)status
        );
        return;
    }

    if (value_size == 0U) {
        wprintf(
            L"  property_value[%lu] name=%ls status=unavailable reason=zero_size\n",
            (unsigned long)index,
            name
        );
        return;
    }

    if (value_size > (ULONG)sizeof(value)) {
        wprintf(
            L"  property_value[%lu] name=%ls status=skipped reason=value_too_large size=%lu limit=%lu\n",
            (unsigned long)index,
            name,
            (unsigned long)value_size,
            (unsigned long)sizeof(value)
        );
        return;
    }

    status = TdhGetProperty(
        (PEVENT_RECORD)event_record,
        0U,
        NULL,
        1U,
        &descriptor,
        value_size,
        value
    );

    if (status != ERROR_SUCCESS) {
        wprintf(
            L"  property_value[%lu] name=%ls status=unavailable reason=read_failed tdh_status=%lu\n",
            (unsigned long)index,
            name,
            (unsigned long)status
        );
        return;
    }

    wprintf(
        L"  property_value[%lu] name=%ls ",
        (unsigned long)index,
        name
    );

    switch (in_type) {
    case TDH_INTYPE_UNICODESTRING: {
        size_t content_size;
        size_t displayed_size;
        int terminated;
        ULONG offset;

        if (!comtrace_scan_string(
                value,
                (size_t)value_size,
                sizeof(wchar_t),
                256U,
                &content_size,
                &displayed_size,
                &terminated)) {
            wprintf(L"status=unavailable reason=invalid_unicode_size\n");
            return;
        }

        wprintf(L"value=\"");

        for (offset = 0U; offset < (ULONG)displayed_size;
             offset += (ULONG)sizeof(wchar_t)) {
            wchar_t character;

            CopyMemory(&character, value + offset, sizeof(character));

            switch (character) {
            case L'\\':
                wprintf(L"\\\\");
                break;
            case L'"':
                wprintf(L"\\\"");
                break;
            case L'\n':
                wprintf(L"\\n");
                break;
            case L'\r':
                wprintf(L"\\r");
                break;
            case L'\t':
                wprintf(L"\\t");
                break;
            default:
                if ((unsigned int)character < 0x20U ||
                    (unsigned int)character == 0x7FU) {
                    wprintf(L"\\u%04x", (unsigned int)character);
                } else {
                    wprintf(L"%lc", character);
                }
                break;
            }
        }

        if (displayed_size == 256U &&
            value_size > (ULONG)displayed_size) {
            wprintf(
                L"...(+%lu bytes)",
                (unsigned long)(value_size - (ULONG)displayed_size)
            );
        }

        if (!terminated) {
            wprintf(L" [unterminated]");
        }

        wprintf(L"\"\n");
        return;
    }

    case TDH_INTYPE_ANSISTRING: {
        size_t content_size;
        size_t displayed_size;
        int terminated;
        size_t offset;

        if (!comtrace_scan_string(
                value,
                (size_t)value_size,
                1U,
                256U,
                &content_size,
                &displayed_size,
                &terminated)) {
            wprintf(L"status=unavailable reason=invalid_string_size\n");
            return;
        }

        wprintf(L"value=\"");

        for (offset = 0U; offset < displayed_size; ++offset) {
            BYTE character = value[offset];

            switch (character) {
            case '\\':
                wprintf(L"\\\\");
                break;
            case '"':
                wprintf(L"\\\"");
                break;
            case '\n':
                wprintf(L"\\n");
                break;
            case '\r':
                wprintf(L"\\r");
                break;
            case '\t':
                wprintf(L"\\t");
                break;
            default:
                if (character >= 0x20U && character <= 0x7EU) {
                    wprintf(L"%c", (int)character);
                } else {
                    wprintf(L"\\x%02x", (unsigned int)character);
                }
                break;
            }
        }

        if (displayed_size == 256U &&
            value_size > (ULONG)displayed_size) {
            wprintf(
                L"...(+%lu bytes)",
                (unsigned long)(value_size - (ULONG)displayed_size)
            );
        }

        if (!terminated) {
            wprintf(L" [unterminated]");
        }

        wprintf(L"\"\n");
        return;
    }

    case TDH_INTYPE_INT8: {
        signed char number;
        if (!comtrace_value_size_is_valid(COMTRACE_VALUE_INT8, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            return;
        }
        CopyMemory(&number, value, sizeof(number));
        wprintf(L"value=%d\n", (int)number);
        return;
    }

    case TDH_INTYPE_UINT8: {
        BYTE number;
        if (!comtrace_value_size_is_valid(COMTRACE_VALUE_UINT8, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            return;
        }
        CopyMemory(&number, value, sizeof(number));
        wprintf(L"value=%u\n", (unsigned int)number);
        return;
    }

    case TDH_INTYPE_INT16: {
        SHORT number;
        if (!comtrace_value_size_is_valid(COMTRACE_VALUE_INT16, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            return;
        }
        CopyMemory(&number, value, sizeof(number));
        wprintf(L"value=%d\n", (int)number);
        return;
    }

    case TDH_INTYPE_UINT16: {
        USHORT number;
        if (!comtrace_value_size_is_valid(COMTRACE_VALUE_UINT16, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            return;
        }
        CopyMemory(&number, value, sizeof(number));
        wprintf(L"value=%u\n", (unsigned int)number);
        return;
    }

    case TDH_INTYPE_INT32: {
        LONG number;
        if (!comtrace_value_size_is_valid(COMTRACE_VALUE_INT32, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            return;
        }
        CopyMemory(&number, value, sizeof(number));
        wprintf(L"value=%ld\n", number);
        return;
    }

    case TDH_INTYPE_UINT32: {
        ULONG number;
        if (!comtrace_value_size_is_valid(COMTRACE_VALUE_UINT32, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            return;
        }
        CopyMemory(&number, value, sizeof(number));
        wprintf(L"value=%lu\n", number);
        return;
    }

    case TDH_INTYPE_INT64: {
        LONGLONG number;
        if (!comtrace_value_size_is_valid(COMTRACE_VALUE_INT64, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            return;
        }
        CopyMemory(&number, value, sizeof(number));
        wprintf(L"value=%lld\n", number);
        return;
    }

    case TDH_INTYPE_UINT64: {
        ULONGLONG number;
        if (!comtrace_value_size_is_valid(COMTRACE_VALUE_UINT64, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            return;
        }
        CopyMemory(&number, value, sizeof(number));
        wprintf(L"value=%llu\n", number);
        return;
    }

    case TDH_INTYPE_FLOAT: {
        float number;
        if (!comtrace_value_size_is_valid(COMTRACE_VALUE_FLOAT, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            return;
        }
        CopyMemory(&number, value, sizeof(number));
        wprintf(L"value=%.9g\n", (double)number);
        return;
    }

    case TDH_INTYPE_DOUBLE: {
        double number;
        if (!comtrace_value_size_is_valid(COMTRACE_VALUE_DOUBLE, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            return;
        }
        CopyMemory(&number, value, sizeof(number));
        wprintf(L"value=%.17g\n", number);
        return;
    }

    case TDH_INTYPE_BOOLEAN: {
        ULONGLONG number = 0U;
        if (!comtrace_value_size_is_valid(
                COMTRACE_VALUE_BOOLEAN, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            return;
        }
        CopyMemory(&number, value, value_size);
        wprintf(L"value=%ls\n", number == 0U ? L"false" : L"true");
        return;
    }

    case TDH_INTYPE_GUID:
        if (!comtrace_value_size_is_valid(
                COMTRACE_VALUE_GUID, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            return;
        } else {
            GUID guid;
            CopyMemory(&guid, value, sizeof(guid));
            wprintf(L"value=");
            print_guid(&guid);
            wprintf(L"\n");
            return;
        }

    case TDH_INTYPE_HEXINT32:
        if (!comtrace_value_size_is_valid(
                COMTRACE_VALUE_HEXINT32, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            return;
        } else {
            ULONGLONG number = 0U;
            CopyMemory(&number, value, sizeof(ULONG));
            wprintf(L"value=0x%llx\n", number);
            return;
        }

    case TDH_INTYPE_HEXINT64:
        if (!comtrace_value_size_is_valid(
                COMTRACE_VALUE_HEXINT64, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            return;
        } else {
            ULONGLONG number = 0U;
            CopyMemory(&number, value, sizeof(number));
            wprintf(L"value=0x%llx\n", number);
            return;
        }

    case TDH_INTYPE_POINTER:
    case TDH_INTYPE_SIZET:
        if (!comtrace_value_size_is_valid(
                in_type == TDH_INTYPE_POINTER
                    ? COMTRACE_VALUE_POINTER
                    : COMTRACE_VALUE_SIZET,
                value_size,
                sizeof(ULONG_PTR))) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            return;
        } else {
            ULONG_PTR number = 0U;
            CopyMemory(&number, value, sizeof(number));
            wprintf(L"value=0x%llx\n", (unsigned long long)number);
            return;
        }

    case TDH_INTYPE_FILETIME:
        if (!comtrace_value_size_is_valid(
                COMTRACE_VALUE_FILETIME, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            return;
        } else {
            ULONGLONG ticks;
            CopyMemory(&ticks, value, sizeof(ticks));
            wprintf(L"value=filetime_ticks:%llu\n", ticks);
            return;
        }

    case TDH_INTYPE_BINARY:
    case TDH_INTYPE_HEXDUMP: {
        ULONG displayed = value_size > 64U ? 64U : value_size;
        ULONG offset;

        wprintf(L"value=hex:");

        for (offset = 0U; offset < displayed; ++offset) {
            wprintf(L"%02x", (unsigned int)value[offset]);
        }

        if (displayed < value_size) {
            wprintf(
                L"...(+%lu bytes)",
                (unsigned long)(value_size - displayed)
            );
        }

        wprintf(L"\n");
        return;
    }

    default:
        wprintf(L"status=skipped reason=unsupported_type\n");
        return;
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
        if (index < 8U) {
            print_event_property_value(event_record, property, name, index);
        } else if (index == 8U) {
            wprintf(L"  payload_value_limit=8 remaining=%lu\n" , (unsigned long)(property_count - 8U));
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
