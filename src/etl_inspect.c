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
#include "com_event.h"
#include "ole_event.h"
#include "event_record.h"
#include "event_catalog.h"
#include "com_identity.h"
#include "etl_decoder.h"
#include "com_timeline.h"
#include "relationship_graph.h"
#include "ole_timeline.h"

#define MAX_SAMPLE_EVENTS 10000UL

typedef struct {
    ULONGLONG total_events;
    ULONGLONG processing_failures;
    ULONG sample_limit;
    ULONG pointer_size;
    ULONG sampled_events;
    ULONGLONG matched_events;
    BOOL has_filters;
    COMTRACE_ETL_FILTERS filters;
    COMTRACE_EVENT_CATALOG event_catalog;
    COMTRACE_EVENT_RECORD_STATUS record_status;
    COMTRACE_EVENT_CATALOG_STATUS catalog_status;
    COMTRACE_TIMELINE timeline;
    COMTRACE_TIMELINE_STATUS timeline_status;
    COMTRACE_OLE_TIMELINE ole_timeline;
    COMTRACE_OLE_TIMELINE_STATUS ole_timeline_status;
    COMTRACE_RELATIONSHIP_GRAPH relationship_graph;
    COMTRACE_GRAPH_STATUS graph_status;
    ULONGLONG trace_frequency;
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

static int read_event_property_count(
    const EVENT_RECORD *event_record,
    const EVENT_PROPERTY_INFO *properties,
    ULONG property_count,
    const BYTE *buffer,
    ULONG buffer_size,
    const EVENT_PROPERTY_INFO *property,
    ULONG *count)
{
    return comtrace_etl_read_property_count(
        event_record,
        properties,
        property_count,
        buffer,
        buffer_size,
        property,
        count
    );
}

static void print_event_property_value_element(
    const EVENT_RECORD *event_record,
    const EVENT_PROPERTY_INFO *property,
    const wchar_t *name,
    const wchar_t *const *parent_names,
    const ULONG *parent_indices,
    ULONG parent_count,
    ULONG index,
    ULONG array_index,
    ULONG pointer_size)
{
    COMTRACE_ETL_PROPERTY_VALUE decoded_value;
    COMTRACE_ETL_PROPERTY_READ_STATUS read_status;
    BYTE *value;
    ULONG value_size;
    ULONG flags = (ULONG)property->Flags;
    USHORT in_type = property->nonStructType.InType;
    wchar_t display_name[256];
    wchar_t part[32];
    ULONG path_index;

    ZeroMemory(&decoded_value, sizeof(decoded_value));

    display_name[0] = L'\0';
    for (path_index = 0U; path_index < parent_count; ++path_index) {
        if (path_index != 0U) {
            wcsncat_s(display_name, _countof(display_name), L".", _TRUNCATE);
        }
        wcsncat_s(display_name, _countof(display_name), parent_names[path_index], _TRUNCATE);
        if (parent_indices[path_index] != 0xFFFFFFFFUL) {
            _snwprintf_s(part, _countof(part), _TRUNCATE, L"[%lu]", (unsigned long)parent_indices[path_index]);
            wcsncat_s(display_name, _countof(display_name), part, _TRUNCATE);
        }
    }
    if (parent_count != 0U) {
        wcsncat_s(display_name, _countof(display_name), L".", _TRUNCATE);
    }
    wcsncat_s(display_name, _countof(display_name), name, _TRUNCATE);
    if (array_index != 0xFFFFFFFFUL) {
        _snwprintf_s(part, _countof(part), _TRUNCATE, L"[%lu]", (unsigned long)array_index);
        wcsncat_s(display_name, _countof(display_name), part, _TRUNCATE);
    }

    if ((flags & PropertyStruct) != 0U ||
        (flags & PropertyWBEMXmlFragment) != 0U ||
        (flags & PropertyHasCustomSchema) != 0U) {
        wprintf(
            L"  property_value[%lu] name=%ls status=skipped reason=complex_property\n",
            (unsigned long)index,
            display_name
        );
        goto cleanup;
    }

    read_status = comtrace_etl_read_property_value(
        event_record,
        property,
        name,
        parent_names,
        parent_indices,
        parent_count,
        array_index,
        &decoded_value
    );

    if (read_status != COMTRACE_ETL_PROPERTY_READ_SUCCESS) {
        switch (read_status) {
        case COMTRACE_ETL_PROPERTY_READ_UNSUPPORTED:
            wprintf(L"  property_value[%lu] name=%ls status=skipped reason=complex_property\n", (unsigned long)index, display_name);
            break;
        case COMTRACE_ETL_PROPERTY_READ_SIZE_QUERY_FAILED:
            wprintf(L"  property_value[%lu] name=%ls status=unavailable reason=size_query_failed tdh_status=%lu\n", (unsigned long)index, display_name, (unsigned long)decoded_value.tdh_status);
            break;
        case COMTRACE_ETL_PROPERTY_READ_ZERO_SIZE:
            wprintf(L"  property_value[%lu] name=%ls status=unavailable reason=zero_size\n", (unsigned long)index, display_name);
            break;
        case COMTRACE_ETL_PROPERTY_READ_ALLOCATION_FAILED:
            wprintf(L"  property_value[%lu] name=%ls status=unavailable reason=allocation_failed\n", (unsigned long)index, display_name);
            break;
        case COMTRACE_ETL_PROPERTY_READ_FAILED:
            wprintf(L"  property_value[%lu] name=%ls status=unavailable reason=read_failed tdh_status=%lu\n", (unsigned long)index, display_name, (unsigned long)decoded_value.tdh_status);
            break;
        default:
            wprintf(L"  property_value[%lu] name=%ls status=unavailable reason=invalid_argument\n", (unsigned long)index, display_name);
            break;
        }
        goto cleanup;
    }

    value = decoded_value.data;
    value_size = decoded_value.size;

    wprintf(
        L"  property_value[%lu] name=%ls ",
        (unsigned long)index,
        display_name
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
            goto cleanup;
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
        goto cleanup;
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
            goto cleanup;
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
        goto cleanup;
    }

    case TDH_INTYPE_INT8: {
        signed char number;
        if (!comtrace_value_size_is_valid(COMTRACE_VALUE_INT8, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            goto cleanup;
        }
        CopyMemory(&number, value, sizeof(number));
        wprintf(L"value=%d\n", (int)number);
        goto cleanup;
    }

    case TDH_INTYPE_ANSICHAR:
    case TDH_INTYPE_UINT8: {
        BYTE number;
        if (!comtrace_value_size_is_valid(COMTRACE_VALUE_UINT8, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            goto cleanup;
        }
        CopyMemory(&number, value, sizeof(number));
        wprintf(L"value=%u\n", (unsigned int)number);
        goto cleanup;
    }

    case TDH_INTYPE_INT16: {
        SHORT number;
        if (!comtrace_value_size_is_valid(COMTRACE_VALUE_INT16, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            goto cleanup;
        }
        CopyMemory(&number, value, sizeof(number));
        wprintf(L"value=%d\n", (int)number);
        goto cleanup;
    }

    case TDH_INTYPE_UNICODECHAR:
    case TDH_INTYPE_UINT16: {
        USHORT number;
        if (!comtrace_value_size_is_valid(COMTRACE_VALUE_UINT16, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            goto cleanup;
        }
        CopyMemory(&number, value, sizeof(number));
        wprintf(L"value=%u\n", (unsigned int)number);
        goto cleanup;
    }

    case TDH_INTYPE_INT32: {
        LONG number;
        if (!comtrace_value_size_is_valid(COMTRACE_VALUE_INT32, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            goto cleanup;
        }
        CopyMemory(&number, value, sizeof(number));
        wprintf(L"value=%ld\n", number);
        goto cleanup;
    }

    case TDH_INTYPE_UINT32: {
        ULONG number;
        if (!comtrace_value_size_is_valid(COMTRACE_VALUE_UINT32, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            goto cleanup;
        }
        CopyMemory(&number, value, sizeof(number));
        wprintf(L"value=%lu\n", number);
        goto cleanup;
    }

    case TDH_INTYPE_INT64: {
        LONGLONG number;
        if (!comtrace_value_size_is_valid(COMTRACE_VALUE_INT64, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            goto cleanup;
        }
        CopyMemory(&number, value, sizeof(number));
        wprintf(L"value=%lld\n", number);
        goto cleanup;
    }

    case TDH_INTYPE_UINT64: {
        ULONGLONG number;
        if (!comtrace_value_size_is_valid(COMTRACE_VALUE_UINT64, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            goto cleanup;
        }
        CopyMemory(&number, value, sizeof(number));
        wprintf(L"value=%llu\n", number);
        goto cleanup;
    }

    case TDH_INTYPE_FLOAT: {
        float number;
        if (!comtrace_value_size_is_valid(COMTRACE_VALUE_FLOAT, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            goto cleanup;
        }
        CopyMemory(&number, value, sizeof(number));
        wprintf(L"value=%.9g\n", (double)number);
        goto cleanup;
    }

    case TDH_INTYPE_DOUBLE: {
        double number;
        if (!comtrace_value_size_is_valid(COMTRACE_VALUE_DOUBLE, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            goto cleanup;
        }
        CopyMemory(&number, value, sizeof(number));
        wprintf(L"value=%.17g\n", number);
        goto cleanup;
    }

    case TDH_INTYPE_BOOLEAN: {
        ULONGLONG number = 0U;
        if (!comtrace_value_size_is_valid(
                COMTRACE_VALUE_BOOLEAN, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            goto cleanup;
        }
        CopyMemory(&number, value, value_size);
        wprintf(L"value=%ls\n", number == 0U ? L"false" : L"true");
        goto cleanup;
    }

    case TDH_INTYPE_GUID:
        if (!comtrace_value_size_is_valid(
                COMTRACE_VALUE_GUID, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            goto cleanup;
        } else {
            GUID guid;
            CopyMemory(&guid, value, sizeof(guid));
            wprintf(L"value=");
            print_guid(&guid);
            wprintf(L"\n");
            goto cleanup;
        }

    case TDH_INTYPE_HEXINT32:
        if (!comtrace_value_size_is_valid(
                COMTRACE_VALUE_HEXINT32, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            goto cleanup;
        } else {
            ULONGLONG number = 0U;
            CopyMemory(&number, value, sizeof(ULONG));
            wprintf(L"value=0x%llx\n", number);
            goto cleanup;
        }

    case TDH_INTYPE_HEXINT64:
        if (!comtrace_value_size_is_valid(
                COMTRACE_VALUE_HEXINT64, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            goto cleanup;
        } else {
            ULONGLONG number = 0U;
            CopyMemory(&number, value, sizeof(number));
            wprintf(L"value=0x%llx\n", number);
            goto cleanup;
        }

    case TDH_INTYPE_POINTER:
    case TDH_INTYPE_SIZET:
        if (!comtrace_value_size_is_valid(
                in_type == TDH_INTYPE_POINTER
                    ? COMTRACE_VALUE_POINTER
                    : COMTRACE_VALUE_SIZET,
                value_size,
                pointer_size)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            goto cleanup;
        } else {
            ULONGLONG number = 0U;
            CopyMemory(&number, value, value_size);
            wprintf(L"value=0x%llx\n", number);
            goto cleanup;
        }

    case TDH_INTYPE_SYSTEMTIME: {
        ComTraceSystemTime system_time;
        if (!comtrace_decode_systemtime(value, value_size, &system_time)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            goto cleanup;
        }
        wprintf(L"value=%04u-%02u-%02uT%02u:%02u:%02u.%03u\n", system_time.year, system_time.month, system_time.day, system_time.hour, system_time.minute, system_time.second, system_time.milliseconds);
        goto cleanup;
    }

    case TDH_INTYPE_FILETIME:
        if (!comtrace_value_size_is_valid(
                COMTRACE_VALUE_FILETIME, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            goto cleanup;
        } else {
            ULONGLONG ticks;
            CopyMemory(&ticks, value, sizeof(ticks));
            wprintf(L"value=filetime_ticks:%llu\n", ticks);
            goto cleanup;
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
        goto cleanup;
    }

    default: {
        ULONG displayed = value_size > 64U ? 64U : value_size;
        ULONG offset;
        wprintf(L"value=hex:");
        for (offset = 0U; offset < displayed; ++offset) {
            wprintf(L"%02x", (unsigned int)value[offset]);
        }
        if (displayed < value_size) {
            wprintf(L"...(+%lu bytes)", (unsigned long)(value_size - displayed));
        }
        wprintf(L"\n");
        goto cleanup;
    }
    }
cleanup:
    comtrace_etl_property_value_destroy(&decoded_value);
}

static void print_event_property_value(
    const EVENT_RECORD *event_record,
    const EVENT_PROPERTY_INFO *property,
    const wchar_t *name,
    ULONG index,
    ULONG pointer_size,
    const EVENT_PROPERTY_INFO *properties,
    ULONG property_count,
    const BYTE *buffer,
    ULONG buffer_size,
    const wchar_t *const *parent_names,
    const ULONG *parent_indices,
    ULONG parent_count)
{
    ULONG flags = (ULONG)property->Flags;
    ULONG element_index;

    if ((flags & PropertyStruct) != 0U) {
        ULONG member_start = property->structType.StructStartIndex;
        ULONG member_count = property->structType.NumOfStructMembers;
        ULONG member_index;
        ULONG struct_count = 1U;
        ULONG struct_index;
        const wchar_t *nested_names[8];
        ULONG nested_indices[8];

        if (parent_count >= 8U) {
            wprintf(L"  property_value[%lu] name=%ls status=unavailable reason=struct_depth_limit\n", (unsigned long)index, name);
            return;
        }
        if (member_start > property_count || member_count > property_count - member_start) {
            wprintf(L"  property_value[%lu] name=%ls status=unavailable reason=invalid_struct_members\n", (unsigned long)index, name);
            return;
        }
        if ((flags & PropertyParamCount) != 0U) {
            if (!read_event_property_count(event_record, properties, property_count, buffer, buffer_size, property, &struct_count)) {
                wprintf(L"  property_value[%lu] name=%ls status=unavailable reason=invalid_count_property\n", (unsigned long)index, name);
                return;
            }
        } else if ((flags & PropertyParamFixedCount) != 0U) {
            struct_count = (ULONG)property->count;
        }
        if (struct_count > 4096U) {
            wprintf(L"  property_value[%lu] name=%ls status=unavailable reason=struct_count_limit\n", (unsigned long)index, name);
            return;
        }
        for (element_index = 0U; element_index < parent_count; ++element_index) {
            nested_names[element_index] = parent_names[element_index];
            nested_indices[element_index] = parent_indices[element_index];
        }
        nested_names[parent_count] = name;

        for (struct_index = 0U; struct_index < struct_count; ++struct_index) {
            nested_indices[parent_count] =
                struct_count == 1U && (flags & (PropertyParamCount | PropertyParamFixedCount)) == 0U
                    ? 0xFFFFFFFFUL
                    : struct_index;
            for (member_index = member_start; member_index < member_start + member_count; ++member_index) {
                const EVENT_PROPERTY_INFO *member = &properties[member_index];
                ULONG name_offset = member->NameOffset;
                ULONG remaining;
                ULONG name_length = 0U;
                const wchar_t *member_name;

                if (name_offset >= buffer_size || (name_offset % (ULONG)sizeof(wchar_t)) != 0U) {
                    wprintf(L"  property_value[%lu] name=%ls status=unavailable reason=invalid_member_name\n", (unsigned long)index, name);
                    continue;
                }
                member_name = (const wchar_t *)(buffer + name_offset);
                remaining = (buffer_size - name_offset) / (ULONG)sizeof(wchar_t);
                while (name_length < remaining && member_name[name_length] != L'\0') {
                    ++name_length;
                }
                if (name_length == remaining) {
                    wprintf(L"  property_value[%lu] name=%ls status=unavailable reason=unterminated_member_name\n", (unsigned long)index, name);
                    continue;
                }
                print_event_property_value(
                    event_record,
                    member,
                    member_name,
                    index,
                    pointer_size,
                    properties,
                    property_count,
                    buffer,
                    buffer_size,
                    nested_names,
                    nested_indices,
                    parent_count + 1U
                );
            }
        }
        return;
    }

    if ((flags & PropertyParamCount) != 0U) {
        ULONG count;
        if (!read_event_property_count(event_record, properties, property_count, buffer, buffer_size, property, &count)) {
            wprintf(L"  property_value[%lu] name=%ls status=unavailable reason=invalid_count_property\n", (unsigned long)index, name);
            return;
        }
        for (element_index = 0U; element_index < count; ++element_index) {
            print_event_property_value_element(
                event_record, property, name, parent_names, parent_indices,
                parent_count, index, element_index, pointer_size
            );
        }
        return;
    }

    if ((flags & PropertyParamFixedCount) != 0U) {
        for (element_index = 0U; element_index < (ULONG)property->count; ++element_index) {
            print_event_property_value_element(
                event_record, property, name, parent_names, parent_indices,
                parent_count, index, element_index, pointer_size
            );
        }
        return;
    }

    print_event_property_value_element(
        event_record, property, name, parent_names, parent_indices,
        parent_count, index, 0xFFFFFFFFUL, pointer_size
    );
}

static void print_event_properties(const EVENT_RECORD *event_record, ULONG pointer_size)
{
    ULONG buffer_size = 0U;
    ULONG allocated_size;
    ULONG status;
    ULONG available_count;
    ULONG property_count;
    ULONG top_level_property_count;
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

    allocated_size = buffer_size;
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

    if (status != ERROR_SUCCESS ||
        buffer_size < property_offset ||
        buffer_size > allocated_size) {
        wprintf(L"  payload_schema=unavailable status=%lu\n", (unsigned long)status);
        free(buffer);
        return;
    }

    event_info = (TRACE_EVENT_INFO *)buffer;
    available_count = (ULONG)((buffer_size - property_offset) / sizeof(EVENT_PROPERTY_INFO));
    property_count = event_info->PropertyCount;
    top_level_property_count = event_info->TopLevelPropertyCount;

    if (property_count > available_count) {
        property_count = available_count;
    }
    if (top_level_property_count > property_count) {
        top_level_property_count = property_count;
    }
    if (top_level_property_count > 64U) {
        top_level_property_count = 64U;
    }

    wprintf(L"  payload_properties=%lu\n", (unsigned long)top_level_property_count);

    for (index = 0U; index < top_level_property_count; ++index) {
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
        if (index < 16U) {
            print_event_property_value(event_record, property, name, index, pointer_size, event_info->EventPropertyInfoArray, property_count, buffer, buffer_size, NULL, NULL, 0U);
        } else if (index == 16U) {
            wprintf(L"  payload_value_limit=16 remaining=%lu\n" , (unsigned long)(top_level_property_count - 16U));
        }
    }

    free(buffer);
}

static void print_ole_event_classification(const EVENT_RECORD *event_record)
{
    const wchar_t *operation = NULL;
    const wchar_t *phase = NULL;

    if (event_record == NULL) {
        return;
    }

    if (comtrace_classify_ole_event(
            &event_record->EventHeader.ProviderId,
            event_record->EventHeader.EventDescriptor.Id,
            event_record->EventHeader.EventDescriptor.Version,
            event_record->EventHeader.EventDescriptor.Opcode,
            &operation,
            &phase)) {
        wprintf(L"  ole_event=%ls phase=%ls\n", operation, phase);
    }
}
static void print_com_process_image_path(
    const wchar_t *label,
    DWORD process_id
)
{
    wchar_t path[1024];

    if (comtrace_get_process_image_path(
            process_id,
            path,
            (DWORD)_countof(path)
        )) {
        wprintf(L"  %ls=%ls\n", label, path);
    }
}

static void print_com_event_classification(const EVENT_RECORD *event_record)
{
    const wchar_t *operation = NULL;
    const wchar_t *phase = NULL;

    if (event_record == NULL) {
        return;
    }

    if (comtrace_classify_com_event(
            &event_record->EventHeader.ProviderId,
            event_record->EventHeader.EventDescriptor.Id,
            event_record->EventHeader.EventDescriptor.Version,
            event_record->EventHeader.EventDescriptor.Opcode,
            &operation,
            &phase)) {
        GUID value;
        ULONG peer_value;
        COMTRACE_COM_OBJECT_IDENTITY object_identity;
        COMTRACE_IDENTITY_STATUS identity_status;

        wprintf(L"  com_event=%ls phase=%ls\n", operation, phase);

        print_com_process_image_path(
            L"com_process_image",
            event_record->EventHeader.ProcessId
        );

        if (comtrace_extract_com_object_identity(
                event_record,
                &object_identity
            ) == COMTRACE_IDENTITY_SUCCESS) {
            if (object_identity.has_ipid) {
                wprintf(L"  com_ipid=");
                print_guid(&object_identity.ipid);
                wprintf(L"\n");
            }
            if (object_identity.has_oid) {
                wprintf(L"  com_oid=0x%llx\n", (unsigned long long)object_identity.oid);
            }
            if (object_identity.has_call_trace_id) {
                wprintf(L"  com_call_trace_id=0x%llx\n", (unsigned long long)object_identity.call_trace_id);
            }
        }

        identity_status = comtrace_extract_guid_property(
            event_record,
            L"ClassId",
            &value
        );
        if (identity_status == COMTRACE_IDENTITY_PROPERTY_NOT_FOUND) {
            identity_status = comtrace_extract_guid_property(
                event_record,
                L"CLSID",
                &value
            );
        }
        if (identity_status == COMTRACE_IDENTITY_SUCCESS) {
            wprintf(L"  com_clsid=");
            print_guid(&value);
            wprintf(L"\n");
        }

        if (comtrace_extract_guid_property(
                event_record,
                L"TargetInterface",
                &value
            ) == COMTRACE_IDENTITY_SUCCESS) {
            wprintf(L"  com_iid=");
            print_guid(&value);
            wprintf(L"\n");
        }

        identity_status = comtrace_extract_uint32_property(
            event_record,
            L"TargetProcessId",
            &peer_value
        );
        if (identity_status == COMTRACE_IDENTITY_SUCCESS) {
            wprintf(L"  com_target_pid=%lu\n", (unsigned long)peer_value);
            print_com_process_image_path(
                L"com_target_process_image",
                peer_value
            );
        }

        identity_status = comtrace_extract_uint32_property(
            event_record,
            L"TargetThreadId",
            &peer_value
        );
        if (identity_status == COMTRACE_IDENTITY_SUCCESS) {
            wprintf(L"  com_target_tid=%lu\n", (unsigned long)peer_value);
        }

        identity_status = comtrace_extract_uint32_property(
            event_record,
            L"SourceProcessId",
            &peer_value
        );
        if (identity_status == COMTRACE_IDENTITY_SUCCESS) {
            wprintf(L"  com_source_pid=%lu\n", (unsigned long)peer_value);
            print_com_process_image_path(
                L"com_source_process_image",
                peer_value
            );
        }

        identity_status = comtrace_extract_uint32_property(
            event_record,
            L"SourceThreadId",
            &peer_value
        );
        if (identity_status == COMTRACE_IDENTITY_SUCCESS) {
            wprintf(L"  com_source_tid=%lu\n", (unsigned long)peer_value);
        }

        {
            COMTRACE_ACTIVITY_IDS activity_ids;
            if (comtrace_extract_activity_ids(event_record, &activity_ids) ==
                COMTRACE_IDENTITY_SUCCESS) {
                if (activity_ids.has_activity_id) {
                    wprintf(L"  com_activity_id=");
                    print_guid(&activity_ids.activity_id);
                    wprintf(L"\n");
                }
                if (activity_ids.has_related_activity_id) {
                    wprintf(L"  com_related_activity_id=");
                    print_guid(&activity_ids.related_activity_id);
                    wprintf(L"\n");
                }
            }
        }
    }
}

static void add_com_timeline_event(
    ETL_INSPECT_CONTEXT *context,
    const EVENT_RECORD *event_record
)
{
    const wchar_t *operation = NULL;
    const wchar_t *phase = NULL;
    COMTRACE_TIMELINE_EVENT timeline_event;
    COMTRACE_COM_OBJECT_IDENTITY object_identity;
    COMTRACE_ACTIVITY_IDS activity_ids;
    ULONGLONG call_trace_id = 0ULL;
    ULONG hresult_bits = 0U;
    LONG hresult = 0;
    COMTRACE_IDENTITY_STATUS identity_status;

    if (context == NULL || event_record == NULL) {
        return;
    }

    if (!comtrace_classify_com_event(
            &event_record->EventHeader.ProviderId,
            event_record->EventHeader.EventDescriptor.Id,
            event_record->EventHeader.EventDescriptor.Version,
            event_record->EventHeader.EventDescriptor.Opcode,
            &operation,
            &phase)) {
        return;
    }

    if (wcscmp(phase, L"start") != 0 && wcscmp(phase, L"stop") != 0) {
        return;
    }

    ZeroMemory(&timeline_event, sizeof(timeline_event));
    timeline_event.operation = operation;
    timeline_event.phase = wcscmp(phase, L"start") == 0
        ? COMTRACE_TIMELINE_PHASE_START
        : COMTRACE_TIMELINE_PHASE_STOP;
    timeline_event.process_id = event_record->EventHeader.ProcessId;
    timeline_event.thread_id = event_record->EventHeader.ThreadId;
    timeline_event.timestamp = (ULONGLONG)event_record->EventHeader.TimeStamp.QuadPart;

    identity_status = comtrace_extract_call_trace_id(
        event_record,
        &call_trace_id
    );
    if (identity_status == COMTRACE_IDENTITY_SUCCESS) {
        timeline_event.has_call_trace_id = TRUE;
        timeline_event.call_trace_id = call_trace_id;
    }

    if (comtrace_extract_com_object_identity(
            event_record,
            &object_identity
        ) == COMTRACE_IDENTITY_SUCCESS) {
        timeline_event.object_identity = &object_identity;
    }

    if (comtrace_extract_activity_ids(event_record, &activity_ids) ==
        COMTRACE_IDENTITY_SUCCESS) {
        timeline_event.activity_ids = &activity_ids;
    }

    if (timeline_event.phase == COMTRACE_TIMELINE_PHASE_STOP) {
        identity_status = comtrace_extract_int32_property(
            event_record,
            L"HRESULT",
            &hresult
        );
        if (identity_status == COMTRACE_IDENTITY_SUCCESS) {
            timeline_event.has_hresult = TRUE;
            timeline_event.hresult = (HRESULT)hresult;
        } else if (comtrace_extract_uint32_property(
                       event_record,
                       L"HRESULT",
                       &hresult_bits
                   ) == COMTRACE_IDENTITY_SUCCESS) {
            timeline_event.has_hresult = TRUE;
            timeline_event.hresult = (HRESULT)hresult_bits;
        }
    }

    if (comtrace_timeline_add_event(&context->timeline, &timeline_event) !=
        COMTRACE_TIMELINE_SUCCESS) {
        context->timeline_status = COMTRACE_TIMELINE_ALLOCATION_FAILED;
    }
}

static const wchar_t *com_timeline_match_name(
    COMTRACE_TIMELINE_MATCH_TYPE match_type
)
{
    switch (match_type) {
    case COMTRACE_TIMELINE_MATCH_CALL_TRACE_ID:
        return L"CallTraceId";
    case COMTRACE_TIMELINE_MATCH_ACTIVITY_ID:
        return L"ActivityId";
    case COMTRACE_TIMELINE_MATCH_PROCESS_THREAD:
        return L"process_thread_operation";
    default:
        return L"none";
    }
}

static void print_com_timeline_summary(const ETL_INSPECT_CONTEXT *context)
{
    size_t count;
    size_t index;
    ULONGLONG matched = 0ULL;
    ULONGLONG matched_by_call_trace_id = 0ULL;
    ULONGLONG matched_by_activity_id = 0ULL;
    ULONGLONG matched_by_process_thread = 0ULL;
    ULONGLONG incomplete_starts = 0ULL;
    ULONGLONG unmatched_stops = 0ULL;
    ULONGLONG failed_operations = 0ULL;

    if (context == NULL) {
        return;
    }

    count = comtrace_timeline_count(&context->timeline);

    for (index = 0U; index < count; ++index) {
        const COMTRACE_TIMELINE_RECORD *record =
            comtrace_timeline_get(&context->timeline, index);

        if (record == NULL) {
            continue;
        }

        if (record->has_start && record->has_stop) {
            ++matched;
            if (record->match_type == COMTRACE_TIMELINE_MATCH_CALL_TRACE_ID) {
                ++matched_by_call_trace_id;
            } else if (record->match_type == COMTRACE_TIMELINE_MATCH_ACTIVITY_ID) {
                ++matched_by_activity_id;
            } else if (record->match_type == COMTRACE_TIMELINE_MATCH_PROCESS_THREAD) {
                ++matched_by_process_thread;
            }
        } else if (record->has_start) {
            ++incomplete_starts;
        } else if (record->has_stop) {
            ++unmatched_stops;
        }

        if (record->has_stop && record->has_hresult && record->hresult < 0) {
            ++failed_operations;
        }
    }

    wprintf(L"\nCOM timeline summary\n");
    wprintf(L"Operations tracked: %llu\n", (unsigned long long)count);
    wprintf(L"Matched start/stop: %llu\n", (unsigned long long)matched);
    wprintf(
        L"Matched by CallTraceId: %llu\n",
        (unsigned long long)matched_by_call_trace_id
    );
    wprintf(
        L"Matched by ActivityId: %llu\n",
        (unsigned long long)matched_by_activity_id
    );
    wprintf(
        L"Matched by process/thread/operation: %llu\n",
        (unsigned long long)matched_by_process_thread
    );
    wprintf(L"Incomplete starts: %llu\n", (unsigned long long)incomplete_starts);
    wprintf(L"Unmatched stops: %llu\n", (unsigned long long)unmatched_stops);
    wprintf(
        L"Missing CallTraceId: %llu\n",
        (unsigned long long)context->timeline.missing_call_trace_ids
    );
    wprintf(
        L"Missing ActivityId: %llu\n",
        (unsigned long long)context->timeline.missing_activity_ids
    );
    wprintf(L"Failed operations: %llu\n", (unsigned long long)failed_operations);

    for (index = 0U; index < count && index < 100U; ++index) {
        const COMTRACE_TIMELINE_RECORD *record =
            comtrace_timeline_get(&context->timeline, index);

        if (record == NULL) {
            continue;
        }

        wprintf(
            L"  sequence=%llu operation=%ls pid=%lu tid=%lu",
            (unsigned long long)(index + 1U),
            record->operation,
            (unsigned long)record->process_id,
            (unsigned long)record->thread_id
        );

        if (record->has_start && record->has_stop) {
            wprintf(
                L" match=%ls",
                com_timeline_match_name(record->match_type)
            );
        }

        if (record->has_call_trace_id) {
            wprintf(L" call_trace_id=0x%llx", (unsigned long long)record->call_trace_id);
        }

        if (record->has_start) {
            wprintf(L" start=%llu", (unsigned long long)record->start_timestamp);
        }
        if (record->has_stop) {
            wprintf(L" stop=%llu", (unsigned long long)record->stop_timestamp);
        }
        if (record->has_duration) {
            if (context->trace_frequency != 0ULL) {
                double duration_ms =
                    1000.0 * (double)record->duration_ticks /
                    (double)context->trace_frequency;
                wprintf(L" duration_ms=%.3f", duration_ms);
            } else {
                wprintf(L" duration_ticks=%llu", (unsigned long long)record->duration_ticks);
            }
        }
        if (record->has_hresult) {
            wprintf(
                L" hresult=0x%08lx result=%ls",
                (unsigned long)record->hresult,
                record->hresult < 0 ? L"failed" : L"succeeded"
            );
        }
        if (record->has_object_identity) {
            if (record->object_identity.has_clsid) {
                wprintf(L" clsid=");
                print_guid(&record->object_identity.clsid);
            }
            if (record->object_identity.has_iid) {
                wprintf(L" iid=");
                print_guid(&record->object_identity.iid);
            }
            if (record->object_identity.has_ipid) {
                wprintf(L" ipid=");
                print_guid(&record->object_identity.ipid);
            }
            if (record->object_identity.has_oid) {
                wprintf(L" oid=0x%llx", (unsigned long long)record->object_identity.oid);
            }
        }
        if (record->has_activity_ids) {
            if (record->activity_ids.has_activity_id) {
                wprintf(L" activity_id=");
                print_guid(&record->activity_ids.activity_id);
            }
            if (record->activity_ids.has_related_activity_id) {
                wprintf(L" related_activity_id=");
                print_guid(&record->activity_ids.related_activity_id);
            }
        }
        wprintf(L"\n");
    }

    if (count > 100U) {
        wprintf(L"  timeline_display_limit=100 remaining=%llu\n", (unsigned long long)(count - 100U));
    }
}

static void add_ole_timeline_event(
    ETL_INSPECT_CONTEXT *context,
    const EVENT_RECORD *event_record
)
{
    const wchar_t *operation = NULL;
    const wchar_t *phase = NULL;
    COMTRACE_ACTIVITY_IDS activity_ids;
    COMTRACE_OLE_TIMELINE_EVENT timeline_event;

    if (context == NULL || event_record == NULL ||
        !comtrace_classify_ole_event(
            &event_record->EventHeader.ProviderId,
            event_record->EventHeader.EventDescriptor.Id,
            event_record->EventHeader.EventDescriptor.Version,
            event_record->EventHeader.EventDescriptor.Opcode,
            &operation,
            &phase)) {
        return;
    }

    ZeroMemory(&timeline_event, sizeof(timeline_event));
    ZeroMemory(&activity_ids, sizeof(activity_ids));

    if (comtrace_extract_activity_ids(event_record, &activity_ids) !=
        COMTRACE_IDENTITY_SUCCESS) {
        timeline_event.activity_ids = NULL;
    } else {
        timeline_event.activity_ids = &activity_ids;
    }

    timeline_event.operation = operation;
    timeline_event.phase = wcscmp(phase, L"Start") == 0
        ? COMTRACE_OLE_TIMELINE_PHASE_START
        : COMTRACE_OLE_TIMELINE_PHASE_STOP;
    timeline_event.process_id = event_record->EventHeader.ProcessId;
    timeline_event.thread_id = event_record->EventHeader.ThreadId;
    timeline_event.timestamp = (ULONGLONG)event_record->EventHeader.TimeStamp.QuadPart;

    if (comtrace_ole_timeline_add_event(
            &context->ole_timeline,
            &timeline_event
        ) != COMTRACE_OLE_TIMELINE_SUCCESS) {
        context->ole_timeline_status =
            COMTRACE_OLE_TIMELINE_ALLOCATION_FAILED;
    }
}

static const wchar_t *ole_timeline_match_name(
    COMTRACE_OLE_TIMELINE_MATCH_TYPE match_type
)
{
    switch (match_type) {
    case COMTRACE_OLE_TIMELINE_MATCH_ACTIVITY_ID:
        return L"ActivityId";
    case COMTRACE_OLE_TIMELINE_MATCH_PROCESS_THREAD:
        return L"process_thread_operation";
    default:
        return L"none";
    }
}

static void print_ole_timeline_summary(
    const ETL_INSPECT_CONTEXT *context
)
{
    size_t count;
    size_t index;
    ULONGLONG matched = 0ULL;
    ULONGLONG matched_by_activity_id = 0ULL;
    ULONGLONG matched_by_process_thread = 0ULL;
    ULONGLONG incomplete = 0ULL;
    ULONGLONG unmatched = 0ULL;
    ULONGLONG clipboard_events = 0ULL;
    ULONGLONG drag_drop_events = 0ULL;

    if (context == NULL) {
        return;
    }

    count = comtrace_ole_timeline_count(&context->ole_timeline);

    for (index = 0U; index < count; ++index) {
        const COMTRACE_OLE_TIMELINE_RECORD *record =
            comtrace_ole_timeline_get(&context->ole_timeline, index);

        if (record == NULL) {
            continue;
        }

        if (record->has_start && record->has_stop) {
            ++matched;
            if (record->match_type == COMTRACE_OLE_TIMELINE_MATCH_ACTIVITY_ID) {
                ++matched_by_activity_id;
            } else if (record->match_type == COMTRACE_OLE_TIMELINE_MATCH_PROCESS_THREAD) {
                ++matched_by_process_thread;
            }
        } else if (record->has_start) {
            ++incomplete;
        } else if (record->has_stop) {
            ++unmatched;
        }

        if (wcsncmp(record->operation, L"OLE_Clipboard_", 14U) == 0) {
            ++clipboard_events;
        } else if (wcsncmp(record->operation, L"OLE_DragDrop_", 13U) == 0) {
            ++drag_drop_events;
        }
    }

    wprintf(L"\nOLE timeline summary\n");
    wprintf(L"Operations tracked: %llu\n", (unsigned long long)count);
    wprintf(L"Clipboard operations: %llu\n", (unsigned long long)clipboard_events);
    wprintf(L"Drag and Drop operations: %llu\n", (unsigned long long)drag_drop_events);
    wprintf(L"Matched start/stop: %llu\n", (unsigned long long)matched);
    wprintf(
        L"Matched by ActivityId: %llu\n",
        (unsigned long long)matched_by_activity_id
    );
    wprintf(
        L"Matched by process/thread/operation: %llu\n",
        (unsigned long long)matched_by_process_thread
    );
    wprintf(L"Incomplete starts: %llu\n", (unsigned long long)incomplete);
    wprintf(L"Unmatched stops: %llu\n", (unsigned long long)unmatched);
    wprintf(
        L"Activity sequences: %llu\n",
        (unsigned long long)comtrace_ole_timeline_sequence_count(&context->ole_timeline)
    );
    wprintf(
        L"Missing ActivityId: %llu\n",
        (unsigned long long)context->ole_timeline.missing_activity_ids
    );

    for (index = 0U; index < count && index < 100U; ++index) {
        const COMTRACE_OLE_TIMELINE_RECORD *record =
            comtrace_ole_timeline_get(&context->ole_timeline, index);

        if (record == NULL) {
            continue;
        }

        wprintf(
            L"  operation=%ls pid=%lu tid=%lu",
            record->operation,
            (unsigned long)record->process_id,
            (unsigned long)record->thread_id
        );

        if (record->has_start && record->has_stop) {
            wprintf(
                L" match=%ls",
                ole_timeline_match_name(record->match_type)
            );
        }

        if (record->has_start) {
            wprintf(L" start=%llu", (unsigned long long)record->start_timestamp);
        }
        if (record->has_stop) {
            wprintf(L" stop=%llu", (unsigned long long)record->stop_timestamp);
        }
        if (record->has_duration) {
            if (context->trace_frequency != 0ULL) {
                double duration_ms =
                    1000.0 * (double)record->duration_ticks /
                    (double)context->trace_frequency;
                wprintf(L" duration_ms=%.3f", duration_ms);
            } else {
                wprintf(L" duration_ticks=%llu", (unsigned long long)record->duration_ticks);
            }
        }
        if (record->has_activity_id) {
            wprintf(L" activity_id=");
            print_guid(&record->activity_id);
        }
        if (record->has_related_activity_id) {
            wprintf(L" related_activity_id=");
            print_guid(&record->related_activity_id);
        }
        wprintf(L"\n");
    }

    if (count > 100U) {
        wprintf(L"  ole_timeline_display_limit=100 remaining=%llu\n", (unsigned long long)(count - 100U));
    }
}

static const wchar_t *relationship_edge_name(COMTRACE_GRAPH_EDGE_TYPE type)
{
    switch (type) {
    case COMTRACE_GRAPH_EDGE_PROCESS_OWNS_THREAD:
        return L"process_owns_thread";
    case COMTRACE_GRAPH_EDGE_PROCESS_CALLS_PROCESS:
        return L"process_calls_process";
    case COMTRACE_GRAPH_EDGE_THREAD_INITIATES_CALL:
        return L"thread_initiates_call";
    case COMTRACE_GRAPH_EDGE_CALL_TARGETS_THREAD:
        return L"call_targets_thread";
    case COMTRACE_GRAPH_EDGE_THREAD_HAS_ACTIVITY:
        return L"thread_has_activity";
    case COMTRACE_GRAPH_EDGE_ACTIVITY_RELATED_TO:
        return L"activity_related_to";
    case COMTRACE_GRAPH_EDGE_CALL_HAS_ACTIVITY:
        return L"call_has_activity";
    case COMTRACE_GRAPH_EDGE_PROCESS_OWNS_OBJECT:
        return L"process_owns_object";
    case COMTRACE_GRAPH_EDGE_CALL_USES_OBJECT:
        return L"call_uses_object";
    case COMTRACE_GRAPH_EDGE_OBJECT_HAS_CLSID:
        return L"object_has_clsid";
    case COMTRACE_GRAPH_EDGE_OBJECT_HAS_IPID:
        return L"object_has_ipid";
    case COMTRACE_GRAPH_EDGE_IPID_SUPPORTS_IID:
        return L"ipid_supports_iid";
    default:
        return L"unknown";
    }
}

static void print_relationship_graph_node(
    const COMTRACE_GRAPH_NODE *node
)
{
    if (node == NULL) {
        wprintf(L"unknown");
        return;
    }

    switch (node->type) {
    case COMTRACE_GRAPH_NODE_PROCESS:
        wprintf(L"process:%lu", (unsigned long)node->process_id);
        break;
    case COMTRACE_GRAPH_NODE_THREAD:
        wprintf(
            L"thread:%lu/%lu",
            (unsigned long)node->process_id,
            (unsigned long)node->thread_id
        );
        break;
    case COMTRACE_GRAPH_NODE_ACTIVITY:
        wprintf(L"activity:");
        print_guid(&node->guid);
        break;
    case COMTRACE_GRAPH_NODE_CALL:
        wprintf(L"call:0x%llx", (unsigned long long)node->value);
        break;
    case COMTRACE_GRAPH_NODE_COM_OBJECT:
        wprintf(
            L"com-object:%lu/0x%llx observations=%llu",
            (unsigned long)node->process_id,
            (unsigned long long)node->value,
            (unsigned long long)node->observation_count
        );
        if (node->has_observed_timestamp) {
            wprintf(
                L" observed_ticks=%lld..%lld",
                (long long)node->first_seen_timestamp,
                (long long)node->last_seen_timestamp
            );
        }
        break;
    case COMTRACE_GRAPH_NODE_CLSID:
        wprintf(L"clsid:");
        print_guid(&node->guid);
        break;
    case COMTRACE_GRAPH_NODE_IID:
        wprintf(L"iid:");
        print_guid(&node->guid);
        break;
    case COMTRACE_GRAPH_NODE_IPID:
        wprintf(L"ipid:");
        print_guid(&node->guid);
        wprintf(L" observations=%llu", (unsigned long long)node->observation_count);
        if (node->has_observed_timestamp) {
            wprintf(
                L" observed_ticks=%lld..%lld",
                (long long)node->first_seen_timestamp,
                (long long)node->last_seen_timestamp
            );
        }
        break;
    default:
        wprintf(L"unknown");
        break;
    }
}

static void add_relationship_graph_event(
    ETL_INSPECT_CONTEXT *context,
    const EVENT_RECORD *event_record
)
{
    const wchar_t *operation = NULL;
    const wchar_t *phase = NULL;
    COMTRACE_COM_OBJECT_IDENTITY identity;
    COMTRACE_ACTIVITY_IDS activity_ids;
    COMTRACE_ACTIVITY_IDS *activity_ids_ptr = NULL;
    int is_com_event;
    int is_ole_event;

    if (context == NULL || event_record == NULL) {
        return;
    }

    is_com_event = comtrace_classify_com_event(
        &event_record->EventHeader.ProviderId,
        event_record->EventHeader.EventDescriptor.Id,
        event_record->EventHeader.EventDescriptor.Version,
        event_record->EventHeader.EventDescriptor.Opcode,
        &operation,
        &phase
    );
    is_ole_event = comtrace_classify_ole_event(
        &event_record->EventHeader.ProviderId,
        event_record->EventHeader.EventDescriptor.Id,
        event_record->EventHeader.EventDescriptor.Version,
        event_record->EventHeader.EventDescriptor.Opcode,
        &operation,
        &phase
    );

    if (!is_com_event && !is_ole_event) {
        return;
    }

    ZeroMemory(&identity, sizeof(identity));

    if (is_com_event) {
        if (comtrace_extract_com_object_identity(event_record, &identity) !=
            COMTRACE_IDENTITY_SUCCESS) {
            return;
        }
    } else {
        identity.process_id = event_record->EventHeader.ProcessId;
        identity.thread_id = event_record->EventHeader.ThreadId;
        identity.has_process_id = identity.process_id != 0U;
        identity.has_thread_id = identity.thread_id != 0U;
    }

    if (comtrace_extract_activity_ids(event_record, &activity_ids) ==
        COMTRACE_IDENTITY_SUCCESS) {
        activity_ids_ptr = &activity_ids;
    }

    if (comtrace_relationship_graph_add_event(
            &context->relationship_graph,
            &identity,
            activity_ids_ptr
        ) != COMTRACE_GRAPH_SUCCESS) {
        context->graph_status = COMTRACE_GRAPH_ALLOCATION_FAILED;
    }
}

static void print_relationship_graph_summary(
    const COMTRACE_RELATIONSHIP_GRAPH *graph
)
{
    size_t index;
    size_t displayed;

    if (graph == NULL) {
        return;
    }

    wprintf(L"\nRelationship graph summary\n");
    wprintf(
        L"Nodes: processes=%llu threads=%llu activities=%llu calls=%llu objects=%llu clsids=%llu iids=%llu ipids=%llu\n",
        (unsigned long long)comtrace_relationship_graph_node_count(graph, COMTRACE_GRAPH_NODE_PROCESS),
        (unsigned long long)comtrace_relationship_graph_node_count(graph, COMTRACE_GRAPH_NODE_THREAD),
        (unsigned long long)comtrace_relationship_graph_node_count(graph, COMTRACE_GRAPH_NODE_ACTIVITY),
        (unsigned long long)comtrace_relationship_graph_node_count(graph, COMTRACE_GRAPH_NODE_CALL),
        (unsigned long long)comtrace_relationship_graph_node_count(graph, COMTRACE_GRAPH_NODE_COM_OBJECT),
        (unsigned long long)comtrace_relationship_graph_node_count(graph, COMTRACE_GRAPH_NODE_CLSID),
        (unsigned long long)comtrace_relationship_graph_node_count(graph, COMTRACE_GRAPH_NODE_IID),
        (unsigned long long)comtrace_relationship_graph_node_count(graph, COMTRACE_GRAPH_NODE_IPID)
    );
    wprintf(
        L"Edges: process_calls_process=%llu thread_initiates_call=%llu call_targets_thread=%llu call_uses_object=%llu related_activity=%llu object_ipid=%llu ipid_iid=%llu\n",
        (unsigned long long)comtrace_relationship_graph_edge_count(graph, COMTRACE_GRAPH_EDGE_PROCESS_CALLS_PROCESS),
        (unsigned long long)comtrace_relationship_graph_edge_count(graph, COMTRACE_GRAPH_EDGE_THREAD_INITIATES_CALL),
        (unsigned long long)comtrace_relationship_graph_edge_count(graph, COMTRACE_GRAPH_EDGE_CALL_TARGETS_THREAD),
        (unsigned long long)comtrace_relationship_graph_edge_count(graph, COMTRACE_GRAPH_EDGE_CALL_USES_OBJECT),
        (unsigned long long)comtrace_relationship_graph_edge_count(graph, COMTRACE_GRAPH_EDGE_ACTIVITY_RELATED_TO),
        (unsigned long long)comtrace_relationship_graph_edge_count(graph, COMTRACE_GRAPH_EDGE_OBJECT_HAS_IPID),
        (unsigned long long)comtrace_relationship_graph_edge_count(graph, COMTRACE_GRAPH_EDGE_IPID_SUPPORTS_IID)
    );

    displayed = graph->edge_count > 100U ? 100U : graph->edge_count;
    for (index = 0U; index < displayed; ++index) {
        const COMTRACE_GRAPH_EDGE *edge =
            comtrace_relationship_graph_get_edge(graph, index);

        if (edge == NULL) {
            continue;
        }

        wprintf(L"  edge=%ls source=", relationship_edge_name(edge->type));
        print_relationship_graph_node(
            comtrace_relationship_graph_get_node(graph, edge->source_index)
        );
        wprintf(L" target=");
        print_relationship_graph_node(
            comtrace_relationship_graph_get_node(graph, edge->target_index)
        );
        wprintf(L"\n");
    }

    if (graph->edge_count > displayed) {
        wprintf(
            L"  graph_display_limit=100 remaining=%llu\n",
            (unsigned long long)(graph->edge_count - displayed)
        );
    }
}

static int event_matches_filters(
    const ETL_INSPECT_CONTEXT *context,
    const EVENT_RECORD *event_record
)
{
    const wchar_t *operation = NULL;
    const wchar_t *phase = NULL;

    if (context == NULL || event_record == NULL) {
        return 0;
    }

    if (!context->has_filters) {
        return 1;
    }

    if (context->filters.has_provider_id &&
        memcmp(
            &event_record->EventHeader.ProviderId,
            &context->filters.provider_id,
            sizeof(GUID)
        ) != 0) {
        return 0;
    }

    if (context->filters.has_event_id &&
        event_record->EventHeader.EventDescriptor.Id != context->filters.event_id) {
        return 0;
    }

    if (context->filters.has_process_id &&
        event_record->EventHeader.ProcessId != context->filters.process_id) {
        return 0;
    }

    if (context->filters.has_thread_id &&
        event_record->EventHeader.ThreadId != context->filters.thread_id) {
        return 0;
    }

    if (context->filters.com_only &&
        !comtrace_classify_com_event(
            &event_record->EventHeader.ProviderId,
            event_record->EventHeader.EventDescriptor.Id,
            event_record->EventHeader.EventDescriptor.Version,
            event_record->EventHeader.EventDescriptor.Opcode,
            &operation,
            &phase
        )) {
        return 0;
    }

    operation = NULL;
    phase = NULL;
    if (context->filters.ole_only &&
        !comtrace_classify_ole_event(
            &event_record->EventHeader.ProviderId,
            event_record->EventHeader.EventDescriptor.Id,
            event_record->EventHeader.EventDescriptor.Version,
            event_record->EventHeader.EventDescriptor.Opcode,
            &operation,
            &phase
        )) {
        return 0;
    }

    return 1;
}

static VOID WINAPI inspect_event_callback(PEVENT_RECORD event_record)
{
    ETL_INSPECT_CONTEXT *context;
    COMTRACE_EVENT_CATALOG_STATUS catalog_status;
    COMTRACE_EVENT_RECORD_STATUS record_status;
    COMTRACE_OWNED_EVENT_RECORD *owned_record = NULL;
    const EVENT_RECORD *owned_event;

    if (event_record == NULL || event_record->UserContext == NULL) {
        return;
    }


    context = (ETL_INSPECT_CONTEXT *)event_record->UserContext;
    ++context->total_events;

    if (!event_matches_filters(context, event_record)) {
        return;
    }

    ++context->matched_events;

    catalog_status = comtrace_event_catalog_add(
        &context->event_catalog,
        event_record
    );

    if (catalog_status != COMTRACE_EVENT_CATALOG_SUCCESS) {
        ++context->processing_failures;
        context->catalog_status = catalog_status;
    }

    add_com_timeline_event(context, event_record);
    add_ole_timeline_event(context, event_record);
    add_relationship_graph_event(context, event_record);

    if (context->sampled_events >= context->sample_limit) {
        return;
    }

    record_status = comtrace_event_record_copy(
        event_record,
        &owned_record
    );

    if (record_status != COMTRACE_EVENT_RECORD_SUCCESS) {
        ++context->processing_failures;
        context->record_status = record_status;
        return;
    }

    owned_event = &owned_record->record;
    ++context->sampled_events;

    wprintf(
        L"sample=%lu time=",
        (unsigned long)context->sampled_events
    );
    print_timestamp(owned_event);

    wprintf(L" provider=");
    print_guid(&owned_event->EventHeader.ProviderId);

    wprintf(
        L" event_id=%u version=%u opcode=%u pid=%lu tid=%lu\n",
        (unsigned int)owned_event->EventHeader.EventDescriptor.Id,
        (unsigned int)owned_event->EventHeader.EventDescriptor.Version,
        (unsigned int)owned_event->EventHeader.EventDescriptor.Opcode,
        (unsigned long)owned_event->EventHeader.ProcessId,
        (unsigned long)owned_event->EventHeader.ThreadId
    );

    print_com_event_classification(owned_event);
    print_ole_event_classification(owned_event);
    print_event_properties(owned_event, context->pointer_size);

    comtrace_event_record_destroy(owned_record);
}
static void destroy_inspect_context(ETL_INSPECT_CONTEXT *context)
{
    if (context == NULL) {
        return;
    }

    comtrace_relationship_graph_destroy(&context->relationship_graph);
    comtrace_ole_timeline_destroy(&context->ole_timeline);
    comtrace_timeline_destroy(&context->timeline);
    comtrace_event_catalog_destroy(&context->event_catalog);
}

static int inspect_etl_internal(
    const wchar_t *etl_path,
    const wchar_t *limit_text,
    const COMTRACE_ETL_FILTERS *filters
)
{
    ETL_INSPECT_CONTEXT context;
    EVENT_TRACE_LOGFILEW logfile;
    TRACEHANDLE trace_handle;
    ULONG process_status;
    ULONG close_status;
    size_t index;
    size_t kind_count;
    DWORD attributes;
    int result = EXIT_FAILURE;

    if (etl_path == NULL || etl_path[0] == L'\0') {
        fwprintf(stderr, L"An ETL input path is required.\n");
        destroy_inspect_context(&context);
        return EXIT_FAILURE;
    }

    ZeroMemory(&context, sizeof(context));
    ZeroMemory(&logfile, sizeof(logfile));
    comtrace_event_catalog_init(&context.event_catalog);
    comtrace_timeline_init(&context.timeline);
    context.timeline_status = COMTRACE_TIMELINE_SUCCESS;
    comtrace_ole_timeline_init(&context.ole_timeline);
    context.ole_timeline_status = COMTRACE_OLE_TIMELINE_SUCCESS;
    comtrace_relationship_graph_init(&context.relationship_graph);
    context.graph_status = COMTRACE_GRAPH_SUCCESS;

    if (filters != NULL) {
        context.has_filters = TRUE;
        context.filters = *filters;
        context.sample_limit = filters->sample_limit;
        if (context.sample_limit == 0U ||
            context.sample_limit > 10000UL ||
            (filters->com_only && filters->ole_only)) {
            fwprintf(stderr, L"Invalid ETL search filters.\n");
            destroy_inspect_context(&context);
        return EXIT_FAILURE;
        }
    } else if (!parse_sample_limit(limit_text, &context.sample_limit)) {
        fwprintf(
            stderr,
            L"Invalid max-events value. Use an integer from 1 to 10000.\n"
        );
        destroy_inspect_context(&context);
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
        destroy_inspect_context(&context);
        return EXIT_FAILURE;
    }

    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0U) {
        fwprintf(stderr, L"ETL input path is a directory.\n");
        destroy_inspect_context(&context);
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
        destroy_inspect_context(&context);
        return EXIT_FAILURE;
    }

    context.pointer_size = logfile.LogfileHeader.PointerSize;
    context.trace_frequency = (ULONGLONG)logfile.LogfileHeader.PerfFreq.QuadPart;
    if (context.pointer_size != 4U && context.pointer_size != 8U) {
        fwprintf(stderr, L"Unsupported trace pointer size: %lu.\n", (unsigned long)context.pointer_size);
        CloseTrace(trace_handle);
        destroy_inspect_context(&context);
        return EXIT_FAILURE;
    }

    wprintf(L"ComTrace 0.2.0\n");
    wprintf(L"%ls\n", context.has_filters ? L"ETL search" : L"ETL inspection");
    wprintf(L"Input: %ls\n", etl_path);
    wprintf(L"Sample limit: %lu\n\n", (unsigned long)context.sample_limit);
    wprintf(L"Event samples:\n");

    process_status = ProcessTrace(&trace_handle, 1U, NULL, NULL);
    close_status = CloseTrace(trace_handle);

    print_com_timeline_summary(&context);
    print_ole_timeline_summary(&context);
    print_relationship_graph_summary(&context.relationship_graph);

    wprintf(L"\nEvent summary\n");
    wprintf(
        L"Events processed: %llu\n",
        (unsigned long long)context.total_events
    );
    wprintf(
        L"Events matching filters: %llu\n",
        (unsigned long long)context.matched_events
    );
    wprintf(
        L"Event samples printed: %lu\n",
        (unsigned long)context.sampled_events
    );
    kind_count = comtrace_event_catalog_count(&context.event_catalog);

    wprintf(
        L"Distinct event types: %llu\n",
        (unsigned long long)kind_count
    );
    wprintf(
        L"Event processing failures: %llu\n",
        (unsigned long long)context.processing_failures
    );

    wprintf(
        L"\nProvider GUID | Event ID | Version | Channel | Level | Opcode | Task | Keyword | Count\n"
    );

    for (index = 0U; index < kind_count; ++index) {
        const COMTRACE_EVENT_KIND *kind =
            comtrace_event_catalog_get(&context.event_catalog, index);

        if (kind == NULL) {
            continue;
        }

        print_guid(&kind->provider_id);

        wprintf(
            L" | %u | %u | %u | %u | %u | %u | 0x%016llX | %llu\n",
            (unsigned int)kind->descriptor.Id,
            (unsigned int)kind->descriptor.Version,
            (unsigned int)kind->descriptor.Channel,
            (unsigned int)kind->descriptor.Level,
            (unsigned int)kind->descriptor.Opcode,
            (unsigned int)kind->descriptor.Task,
            (unsigned long long)kind->descriptor.Keyword,
            (unsigned long long)kind->count
        );
    }
    result = EXIT_SUCCESS;

    if (process_status != ERROR_SUCCESS) {
        fwprintf(
            stderr,
            L"ProcessTrace failed with status %lu.\n",
            (unsigned long)process_status
        );
        result = EXIT_FAILURE;
    }

    if (close_status != ERROR_SUCCESS) {
        fwprintf(
            stderr,
            L"CloseTrace failed with status %lu.\n",
            (unsigned long)close_status
        );
        result = EXIT_FAILURE;
    }

    if (context.record_status != COMTRACE_EVENT_RECORD_SUCCESS) {
        fwprintf(
            stderr,
            L"Event record copy failed with status %u.\n",
            (unsigned int)context.record_status
        );
        result = EXIT_FAILURE;
    }

    if (context.catalog_status != COMTRACE_EVENT_CATALOG_SUCCESS) {
        fwprintf(
            stderr,
            L"Event catalog update failed with status %u.\n",
            (unsigned int)context.catalog_status
        );
        result = EXIT_FAILURE;
    }

    if (context.processing_failures != 0ULL) {
        fwprintf(
            stderr,
            L"Event processing failures: %llu\n",
            (unsigned long long)context.processing_failures
        );
        result = EXIT_FAILURE;
    }

    if (context.timeline_status != COMTRACE_TIMELINE_SUCCESS) {
        fwprintf(stderr, L"COM timeline processing failed.\n");
        result = EXIT_FAILURE;
    }

    if (context.ole_timeline_status != COMTRACE_OLE_TIMELINE_SUCCESS) {
        fwprintf(stderr, L"OLE timeline processing failed.\n");
        result = EXIT_FAILURE;
    }

    if (context.graph_status != COMTRACE_GRAPH_SUCCESS) {
        fwprintf(stderr, L"Relationship graph processing failed.\n");
        result = EXIT_FAILURE;
    }

    destroy_inspect_context(&context);

    return result;
}

int comtrace_inspect_etl(
    const wchar_t *etl_path,
    const wchar_t *limit_text
)
{
    return inspect_etl_internal(etl_path, limit_text, NULL);
}

int comtrace_search_etl(
    const wchar_t *etl_path,
    const COMTRACE_ETL_FILTERS *filters
)
{
    if (filters == NULL) {
        return EXIT_FAILURE;
    }

    return inspect_etl_internal(etl_path, NULL, filters);
}
