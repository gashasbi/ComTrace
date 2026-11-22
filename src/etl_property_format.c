#include "etl_property_format.h"

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

int comtrace_etl_print_tdh_formatted_property(
    const BYTE *metadata_buffer,
    ULONG metadata_size,
    const EVENT_PROPERTY_INFO *property,
    USHORT in_type,
    USHORT out_type,
    const BYTE *value,
    ULONG value_size,
    ULONG pointer_size
)
{
    ULONG formatted_size = 0U;
    ULONG allocated_size;
    ULONG status;
    ULONG character_count;
    ULONG index;
    USHORT property_length;
    USHORT user_data_length;
    USHORT consumed = 0U;
    wchar_t *formatted;

    if (metadata_buffer == NULL ||
        metadata_size < offsetof(TRACE_EVENT_INFO, EventPropertyInfoArray) ||
        property == NULL ||
        value == NULL ||
        value_size == 0U ||
        value_size > USHRT_MAX) {
        return 0;
    }

    property_length = property->length;
    if (property_length == 0U || property_length > value_size) {
        property_length = (USHORT)value_size;
    }
    user_data_length = (USHORT)value_size;

    status = TdhFormatProperty(
        (PTRACE_EVENT_INFO)metadata_buffer,
        NULL,
        pointer_size,
        in_type,
        out_type,
        property_length,
        user_data_length,
        (PBYTE)value,
        &formatted_size,
        NULL,
        &consumed
    );
    if (status != ERROR_INSUFFICIENT_BUFFER ||
        formatted_size < sizeof(wchar_t) ||
        formatted_size > 65536U) {
        return 0;
    }

    allocated_size = formatted_size;
    formatted = (wchar_t *)malloc(formatted_size);
    if (formatted == NULL) {
        return 0;
    }

    status = TdhFormatProperty(
        (PTRACE_EVENT_INFO)metadata_buffer,
        NULL,
        pointer_size,
        in_type,
        out_type,
        property_length,
        user_data_length,
        (PBYTE)value,
        &formatted_size,
        formatted,
        &consumed
    );
    if (status != ERROR_SUCCESS || formatted_size > allocated_size) {
        free(formatted);
        return 0;
    }

    character_count = formatted_size / (ULONG)sizeof(wchar_t);
    for (index = 0U; index < character_count; ++index) {
        if (formatted[index] == L'\0') {
            break;
        }
    }

    wprintf(L"value=\"");
    for (character_count = 0U; character_count < index; ++character_count) {
        wchar_t character = formatted[character_count];
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
    wprintf(L"\" raw=hex:");
    for (index = 0U; index < value_size; ++index) {
        wprintf(L"%02x", (unsigned int)value[index]);
    }
    wprintf(L"\n");
    free(formatted);
    return 1;
}

int comtrace_etl_print_mapped_property_value(
    const EVENT_RECORD *event_record,
    const BYTE *metadata_buffer,
    ULONG metadata_size,
    const EVENT_PROPERTY_INFO *property,
    USHORT in_type,
    USHORT out_type,
    const BYTE *value,
    ULONG value_size,
    ULONG pointer_size
)
{
    ULONG map_name_offset;
    ULONG remaining;
    ULONG name_length = 0U;
    ULONG map_size = 0U;
    ULONG allocated_map_size;
    ULONG formatted_size = 0U;
    ULONG allocated_formatted_size;
    ULONG status;
    ULONG available_entries;
    ULONG character_count;
    ULONG index;
    USHORT property_length;
    USHORT user_data_length;
    USHORT consumed = 0U;
    const wchar_t *map_name;
    EVENT_MAP_INFO *map_info;
    BYTE *map_buffer = NULL;
    wchar_t *formatted = NULL;

    if (event_record == NULL ||
        metadata_buffer == NULL ||
        property == NULL ||
        value == NULL ||
        value_size == 0U ||
        value_size > USHRT_MAX ||
        metadata_size < offsetof(TRACE_EVENT_INFO, EventPropertyInfoArray)) {
        return 0;
    }

    map_name_offset = property->nonStructType.MapNameOffset;
    if (map_name_offset == 0U ||
        map_name_offset >= metadata_size ||
        (map_name_offset % (ULONG)sizeof(wchar_t)) != 0U) {
        return 0;
    }

    map_name = (const wchar_t *)(metadata_buffer + map_name_offset);
    remaining = (metadata_size - map_name_offset) / (ULONG)sizeof(wchar_t);
    while (name_length < remaining && map_name[name_length] != L'\0') {
        ++name_length;
    }
    if (name_length == 0U || name_length == remaining) {
        return 0;
    }

    status = TdhGetEventMapInformation(
        (PEVENT_RECORD)event_record,
        (PWSTR)map_name,
        NULL,
        &map_size
    );
    if (status != ERROR_INSUFFICIENT_BUFFER ||
        map_size < offsetof(EVENT_MAP_INFO, MapEntryArray) ||
        map_size > 1048576U) {
        return 0;
    }

    allocated_map_size = map_size;
    map_buffer = (BYTE *)malloc(map_size);
    if (map_buffer == NULL) {
        return 0;
    }

    status = TdhGetEventMapInformation(
        (PEVENT_RECORD)event_record,
        (PWSTR)map_name,
        (PEVENT_MAP_INFO)map_buffer,
        &map_size
    );
    if (status != ERROR_SUCCESS ||
        map_size < offsetof(EVENT_MAP_INFO, MapEntryArray) ||
        map_size > allocated_map_size) {
        goto cleanup;
    }

    map_info = (EVENT_MAP_INFO *)map_buffer;
    available_entries = (map_size -
        (ULONG)offsetof(EVENT_MAP_INFO, MapEntryArray)) /
        (ULONG)sizeof(EVENT_MAP_ENTRY);
    if (map_info->EntryCount > available_entries) {
        goto cleanup;
    }

    property_length = property->length;
    if (property_length == 0U || property_length > value_size) {
        property_length = (USHORT)value_size;
    }
    user_data_length = (USHORT)value_size;

    status = TdhFormatProperty(
        (PTRACE_EVENT_INFO)metadata_buffer,
        map_info,
        pointer_size,
        in_type,
        out_type,
        property_length,
        user_data_length,
        (PBYTE)value,
        &formatted_size,
        NULL,
        &consumed
    );
    if (status != ERROR_INSUFFICIENT_BUFFER ||
        formatted_size < sizeof(wchar_t) ||
        formatted_size > 65536U) {
        goto cleanup;
    }

    allocated_formatted_size = formatted_size;
    formatted = (wchar_t *)malloc(formatted_size);
    if (formatted == NULL) {
        goto cleanup;
    }

    status = TdhFormatProperty(
        (PTRACE_EVENT_INFO)metadata_buffer,
        map_info,
        pointer_size,
        in_type,
        out_type,
        property_length,
        user_data_length,
        (PBYTE)value,
        &formatted_size,
        formatted,
        &consumed
    );
    if (status != ERROR_SUCCESS ||
        formatted_size > allocated_formatted_size) {
        goto cleanup;
    }

    character_count = formatted_size / (ULONG)sizeof(wchar_t);
    for (index = 0U; index < character_count; ++index) {
        if (formatted[index] == L'\0') {
            break;
        }
    }
    while (index != 0U && formatted[index - 1U] == L' ') {
        --index;
    }

    wprintf(L"value=\"");
    for (character_count = 0U; character_count < index; ++character_count) {
        wchar_t character = formatted[character_count];
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
    wprintf(L"\" raw=hex:");
    for (index = 0U; index < value_size; ++index) {
        wprintf(L"%02x", (unsigned int)value[index]);
    }
    wprintf(L"\n");
    free(formatted);
    free(map_buffer);
    return 1;

cleanup:
    free(formatted);
    free(map_buffer);
    return 0;
}


void comtrace_etl_print_custom_schema_metadata(
    const BYTE *metadata_buffer,
    ULONG metadata_size,
    const EVENT_PROPERTY_INFO *property
)
{
    ULONG offset;
    USHORT schema_size;
    ULONG index;
    USHORT protocol_id;

    if (metadata_buffer == NULL || property == NULL) {
        wprintf(L" custom_schema=unavailable");
        return;
    }

    offset = property->customSchemaType.CustomSchemaOffset;
    if (offset == 0U ||
        offset > metadata_size ||
        metadata_size - offset < 4U) {
        wprintf(L" custom_schema=invalid");
        return;
    }

    CopyMemory(&protocol_id, metadata_buffer + offset, sizeof(protocol_id));
    CopyMemory(&schema_size, metadata_buffer + offset + 2U, sizeof(USHORT));
    if (schema_size > metadata_size - offset - 4U) {
        wprintf(L" custom_schema=invalid");
        return;
    }

    wprintf(
        L" custom_schema_protocol=0x%04X custom_schema_size=%lu custom_schema_data=hex:",
        (unsigned int)protocol_id,
        (unsigned long)schema_size
    );
    for (index = 0U; index < schema_size; ++index) {
        wprintf(L"%02x", (unsigned int)metadata_buffer[offset + 4U + index]);
    }
}

