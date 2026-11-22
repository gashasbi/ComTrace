#include "etl_property_format.h"

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>


const wchar_t *comtrace_etl_input_type_name(USHORT in_type)
{
    switch (in_type) {
    case TDH_INTYPE_NULL: return L"NULL";
    case TDH_INTYPE_UNICODESTRING: return L"UNICODESTRING";
    case TDH_INTYPE_ANSISTRING: return L"ANSISTRING";
    case TDH_INTYPE_INT8: return L"INT8";
    case TDH_INTYPE_UINT8: return L"UINT8";
    case TDH_INTYPE_INT16: return L"INT16";
    case TDH_INTYPE_UINT16: return L"UINT16";
    case TDH_INTYPE_INT32: return L"INT32";
    case TDH_INTYPE_UINT32: return L"UINT32";
    case TDH_INTYPE_INT64: return L"INT64";
    case TDH_INTYPE_UINT64: return L"UINT64";
    case TDH_INTYPE_FLOAT: return L"FLOAT";
    case TDH_INTYPE_DOUBLE: return L"DOUBLE";
    case TDH_INTYPE_BOOLEAN: return L"BOOLEAN";
    case TDH_INTYPE_BINARY: return L"BINARY";
    case TDH_INTYPE_GUID: return L"GUID";
    case TDH_INTYPE_POINTER: return L"POINTER";
    case TDH_INTYPE_FILETIME: return L"FILETIME";
    case TDH_INTYPE_SYSTEMTIME: return L"SYSTEMTIME";
    case TDH_INTYPE_SID: return L"SID";
    case TDH_INTYPE_HEXINT32: return L"HEXINT32";
    case TDH_INTYPE_HEXINT64: return L"HEXINT64";
    case TDH_INTYPE_MANIFEST_COUNTEDSTRING: return L"MANIFEST_COUNTEDSTRING";
    case TDH_INTYPE_MANIFEST_COUNTEDANSISTRING: return L"MANIFEST_COUNTEDANSISTRING";
    case TDH_INTYPE_RESERVED24: return L"RESERVED24";
    case TDH_INTYPE_MANIFEST_COUNTEDBINARY: return L"MANIFEST_COUNTEDBINARY";
    case TDH_INTYPE_COUNTEDSTRING: return L"COUNTEDSTRING";
    case TDH_INTYPE_COUNTEDANSISTRING: return L"COUNTEDANSISTRING";
    case TDH_INTYPE_REVERSEDCOUNTEDSTRING: return L"REVERSEDCOUNTEDSTRING";
    case TDH_INTYPE_REVERSEDCOUNTEDANSISTRING: return L"REVERSEDCOUNTEDANSISTRING";
    case TDH_INTYPE_NONNULLTERMINATEDSTRING: return L"NONNULLTERMINATEDSTRING";
    case TDH_INTYPE_NONNULLTERMINATEDANSISTRING: return L"NONNULLTERMINATEDANSISTRING";
    case TDH_INTYPE_UNICODECHAR: return L"UNICODECHAR";
    case TDH_INTYPE_ANSICHAR: return L"ANSICHAR";
    case TDH_INTYPE_SIZET: return L"SIZET";
    case TDH_INTYPE_HEXDUMP: return L"HEXDUMP";
    case TDH_INTYPE_WBEMSID: return L"WBEMSID";
    default: return L"UNKNOWN";
    }
}

const wchar_t *comtrace_etl_output_type_name(USHORT out_type)
{
    switch (out_type) {
    case TDH_OUTTYPE_NULL: return L"NULL";
    case TDH_OUTTYPE_STRING: return L"STRING";
    case TDH_OUTTYPE_DATETIME: return L"DATETIME";
    case TDH_OUTTYPE_BYTE: return L"BYTE";
    case TDH_OUTTYPE_UNSIGNEDBYTE: return L"UNSIGNEDBYTE";
    case TDH_OUTTYPE_SHORT: return L"SHORT";
    case TDH_OUTTYPE_UNSIGNEDSHORT: return L"UNSIGNEDSHORT";
    case TDH_OUTTYPE_INT: return L"INT";
    case TDH_OUTTYPE_UNSIGNEDINT: return L"UNSIGNEDINT";
    case TDH_OUTTYPE_LONG: return L"LONG";
    case TDH_OUTTYPE_UNSIGNEDLONG: return L"UNSIGNEDLONG";
    case TDH_OUTTYPE_FLOAT: return L"FLOAT";
    case TDH_OUTTYPE_DOUBLE: return L"DOUBLE";
    case TDH_OUTTYPE_BOOLEAN: return L"BOOLEAN";
    case TDH_OUTTYPE_GUID: return L"GUID";
    case TDH_OUTTYPE_HEXBINARY: return L"HEXBINARY";
    case TDH_OUTTYPE_HEXINT8: return L"HEXINT8";
    case TDH_OUTTYPE_HEXINT16: return L"HEXINT16";
    case TDH_OUTTYPE_HEXINT32: return L"HEXINT32";
    case TDH_OUTTYPE_HEXINT64: return L"HEXINT64";
    case TDH_OUTTYPE_PID: return L"PID";
    case TDH_OUTTYPE_TID: return L"TID";
    case TDH_OUTTYPE_PORT: return L"PORT";
    case TDH_OUTTYPE_IPV4: return L"IPV4";
    case TDH_OUTTYPE_IPV6: return L"IPV6";
    case TDH_OUTTYPE_SOCKETADDRESS: return L"SOCKETADDRESS";
    case TDH_OUTTYPE_CIMDATETIME: return L"CIMDATETIME";
    case TDH_OUTTYPE_ETWTIME: return L"ETWTIME";
    case TDH_OUTTYPE_XML: return L"XML";
    case TDH_OUTTYPE_ERRORCODE: return L"ERRORCODE";
    case TDH_OUTTYPE_WIN32ERROR: return L"WIN32ERROR";
    case TDH_OUTTYPE_NTSTATUS: return L"NTSTATUS";
    case TDH_OUTTYPE_HRESULT: return L"HRESULT";
    case TDH_OUTTYPE_CULTURE_INSENSITIVE_DATETIME: return L"CULTURE_INSENSITIVE_DATETIME";
    case TDH_OUTTYPE_JSON: return L"JSON";
    case TDH_OUTTYPE_UTF8: return L"UTF8";
    case TDH_OUTTYPE_PKCS7_WITH_TYPE_INFO: return L"PKCS7_WITH_TYPE_INFO";
    case TDH_OUTTYPE_CODE_POINTER: return L"CODE_POINTER";
    case TDH_OUTTYPE_DATETIME_UTC: return L"DATETIME_UTC";
    case TDH_OUTTYPE_REDUCEDSTRING: return L"REDUCEDSTRING";
    case TDH_OUTTYPE_NOPRINT: return L"NOPRINT";
    default: return L"UNKNOWN";
    }
}

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

