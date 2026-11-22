#include "etl_property_format.h"
#include "etl_value.h"

#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>



int comtrace_etl_normalize_property_payload(
    USHORT *in_type,
    BYTE **value,
    ULONG *value_size,
    ULONG pointer_size
)
{
    size_t payload_offset;
    size_t payload_size;
    int reverse_order = 0;
    size_t prefix_size = 0U;

    if (in_type == NULL || value == NULL || *value == NULL ||
        value_size == NULL) {
        return 0;
    }

    switch (*in_type) {
    case TDH_INTYPE_MANIFEST_COUNTEDSTRING:
    case TDH_INTYPE_COUNTEDSTRING:
        prefix_size = 2U;
        *in_type = TDH_INTYPE_UNICODESTRING;
        break;
    case TDH_INTYPE_REVERSEDCOUNTEDSTRING:
        prefix_size = 2U;
        reverse_order = 1;
        *in_type = TDH_INTYPE_UNICODESTRING;
        break;
    case TDH_INTYPE_NONNULLTERMINATEDSTRING:
        *in_type = TDH_INTYPE_UNICODESTRING;
        break;
    case TDH_INTYPE_MANIFEST_COUNTEDANSISTRING:
    case TDH_INTYPE_COUNTEDANSISTRING:
        prefix_size = 2U;
        *in_type = TDH_INTYPE_ANSISTRING;
        break;
    case TDH_INTYPE_REVERSEDCOUNTEDANSISTRING:
        prefix_size = 2U;
        reverse_order = 1;
        *in_type = TDH_INTYPE_ANSISTRING;
        break;
    case TDH_INTYPE_NONNULLTERMINATEDANSISTRING:
        *in_type = TDH_INTYPE_ANSISTRING;
        break;
    case TDH_INTYPE_MANIFEST_COUNTEDBINARY:
        prefix_size = 2U;
        *in_type = TDH_INTYPE_BINARY;
        break;
    case TDH_INTYPE_HEXDUMP:
        prefix_size = 4U;
        break;
    case TDH_INTYPE_WBEMSID:
        if (!comtrace_decode_wbem_sid_payload(
                (size_t)*value_size,
                (size_t)pointer_size,
                &payload_offset,
                &payload_size)) {
            return 0;
        }
        *value += payload_offset;
        *value_size = (ULONG)payload_size;
        *in_type = TDH_INTYPE_SID;
        return 1;
    default:
        return 1;
    }

    if (prefix_size == 0U) {
        return 1;
    }

    if (!comtrace_decode_length_prefixed_payload(
            *value,
            (size_t)*value_size,
            prefix_size,
            reverse_order,
            &payload_offset,
            &payload_size)) {
        return 0;
    }

    *value += payload_offset;
    *value_size = (ULONG)payload_size;
    return 1;
}

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

    if ((property->Flags & PropertyParamLength) != 0U) {
        property_length = (USHORT)value_size;
    } else if ((property->Flags & PropertyParamFixedLength) != 0U) {
        property_length = property->length;
    } else {
        property_length = property->length;
        if (property_length == 0U || property_length > value_size) {
            property_length = (USHORT)value_size;
        }
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


static const wchar_t *get_map_string(
    const EVENT_MAP_INFO *map_info,
    ULONG map_size,
    ULONG offset,
    ULONG *length,
    int trim_trailing_spaces
)
{
    ULONG remaining;
    ULONG index;
    const wchar_t *value;

    if (map_info == NULL ||
        length == NULL ||
        offset == 0U ||
        offset >= map_size ||
        (offset % (ULONG)sizeof(wchar_t)) != 0U) {
        return NULL;
    }

    value = (const wchar_t *)((const BYTE *)map_info + offset);
    remaining = (map_size - offset) / (ULONG)sizeof(wchar_t);
    for (index = 0U; index < remaining; ++index) {
        if (value[index] == L'\0') {
            *length = index;
            if (trim_trailing_spaces) {
                while (*length != 0U && value[*length - 1U] == L' ') {
                    --(*length);
                }
            }
            return value;
        }
    }

    return NULL;
}

static void print_escaped_map_string(
    const wchar_t *value,
    ULONG length
)
{
    ULONG index;

    for (index = 0U; index < length; ++index) {
        wchar_t character = value[index];
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
}

static int get_map_numeric_value(
    USHORT in_type,
    const BYTE *value,
    ULONG value_size,
    ULONG *number
)
{
    if (value == NULL || number == NULL) {
        return 0;
    }

    switch (in_type) {
    case TDH_INTYPE_INT8: {
        signed char number_value;
        if (value_size != sizeof(number_value)) return 0;
        CopyMemory(&number_value, value, sizeof(number_value));
        *number = (ULONG)(LONG)number_value;
        return 1;
    }
    case TDH_INTYPE_UINT8: {
        BYTE number_value;
        if (value_size != sizeof(number_value)) return 0;
        CopyMemory(&number_value, value, sizeof(number_value));
        *number = (ULONG)number_value;
        return 1;
    }
    case TDH_INTYPE_INT16: {
        SHORT number_value;
        if (value_size != sizeof(number_value)) return 0;
        CopyMemory(&number_value, value, sizeof(number_value));
        *number = (ULONG)(LONG)number_value;
        return 1;
    }
    case TDH_INTYPE_UINT16: {
        USHORT number_value;
        if (value_size != sizeof(number_value)) return 0;
        CopyMemory(&number_value, value, sizeof(number_value));
        *number = (ULONG)number_value;
        return 1;
    }
    case TDH_INTYPE_INT32: {
        LONG number_value;
        if (value_size != sizeof(number_value)) return 0;
        CopyMemory(&number_value, value, sizeof(number_value));
        *number = (ULONG)number_value;
        return 1;
    }
    case TDH_INTYPE_UINT32:
    case TDH_INTYPE_HEXINT32:
    case TDH_INTYPE_BOOLEAN: {
        ULONG number_value;
        if (value_size != sizeof(number_value)) return 0;
        CopyMemory(&number_value, value, sizeof(number_value));
        *number = number_value;
        return 1;
    }
    case TDH_INTYPE_INT64: {
        LONGLONG number_value;
        if (value_size != sizeof(number_value)) return 0;
        CopyMemory(&number_value, value, sizeof(number_value));
        if (number_value < (LONGLONG)LONG_MIN ||
            number_value > (LONGLONG)LONG_MAX) {
            return 0;
        }
        *number = (ULONG)(LONG)number_value;
        return 1;
    }
    case TDH_INTYPE_UINT64:
    case TDH_INTYPE_HEXINT64: {
        ULONGLONG number_value;
        if (value_size != sizeof(number_value)) return 0;
        CopyMemory(&number_value, value, sizeof(number_value));
        if (number_value > (ULONGLONG)ULONG_MAX) return 0;
        *number = (ULONG)number_value;
        return 1;
    }
    case TDH_INTYPE_SIZET:
    case TDH_INTYPE_POINTER: {
        ULONGLONG number_value = 0ULL;
        if (value_size != sizeof(ULONG) &&
            value_size != sizeof(ULONGLONG)) {
            return 0;
        }
        CopyMemory(&number_value, value, value_size);
        if (number_value > (ULONGLONG)ULONG_MAX) {
            return 0;
        }
        *number = (ULONG)number_value;
        return 1;
    }
    default:
        return 0;
    }
}

static int map_string_matches(
    USHORT in_type,
    USHORT out_type,
    const BYTE *value,
    ULONG value_size,
    const wchar_t *map_value,
    ULONG map_value_length
)
{
    wchar_t *converted = NULL;
    const wchar_t *input;
    ULONG input_length;
    int converted_length;
    UINT code_page;

    if (value == NULL || map_value == NULL) {
        return 0;
    }

    if (in_type == TDH_INTYPE_UNICODESTRING) {
        if ((value_size % sizeof(wchar_t)) != 0U) {
            return 0;
        }
        input = (const wchar_t *)value;
        input_length = value_size / (ULONG)sizeof(wchar_t);
        while (input_length != 0U && input[input_length - 1U] == L'\0') {
            --input_length;
        }
    } else if (in_type == TDH_INTYPE_ANSISTRING) {
        if (value_size > (ULONG)INT_MAX) {
            return 0;
        }
        code_page = (out_type == TDH_OUTTYPE_UTF8 ||
            out_type == TDH_OUTTYPE_XML ||
            out_type == TDH_OUTTYPE_JSON)
            ? CP_UTF8
            : CP_ACP;
        converted_length = MultiByteToWideChar(
            code_page,
            code_page == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0U,
            (LPCCH)value,
            (int)value_size,
            NULL,
            0
        );
        if (converted_length <= 0) {
            return 0;
        }
        converted = (wchar_t *)malloc(
            ((size_t)converted_length + 1U) * sizeof(wchar_t)
        );
        if (converted == NULL) {
            return 0;
        }
        if (MultiByteToWideChar(
                code_page,
                code_page == CP_UTF8 ? MB_ERR_INVALID_CHARS : 0U,
                (LPCCH)value,
                (int)value_size,
                converted,
                converted_length
            ) != converted_length) {
            free(converted);
            return 0;
        }
        converted[converted_length] = L'\0';
        input = converted;
        input_length = (ULONG)converted_length;
        while (input_length != 0U && input[input_length - 1U] == L'\0') {
            --input_length;
        }
    } else {
        return 0;
    }

    if (input_length == map_value_length &&
        wmemcmp(input, map_value, input_length) == 0) {
        free(converted);
        return 1;
    }

    free(converted);
    return 0;
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
    ULONG status;
    ULONG available_entries;
    ULONG index;
    ULONG numeric_value = 0U;
    ULONG matched_bits = 0U;
    ULONG matched_count = 0U;
    ULONG string_length;
    ULONG input_offset;
    ULONG output_offset;
    ULONG flags;
    ULONG map_value_length;
    int is_value_map;
    int is_bitmap;
    int matched = 0;
    const wchar_t *map_name;
    const wchar_t *map_string;
    EVENT_MAP_INFO *map_info;
    BYTE *map_buffer = NULL;

    (void)pointer_size;

    if (event_record == NULL ||
        metadata_buffer == NULL ||
        property == NULL ||
        value == NULL ||
        value_size == 0U ||
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
        map_size > COMTRACE_ETL_MAX_METADATA_BYTES) {
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
        free(map_buffer);
        return 0;
    }

    map_info = (EVENT_MAP_INFO *)map_buffer;
    available_entries = (map_size -
        (ULONG)offsetof(EVENT_MAP_INFO, MapEntryArray)) /
        (ULONG)sizeof(EVENT_MAP_ENTRY);
    if (map_info->EntryCount > available_entries) {
        free(map_buffer);
        return 0;
    }

    flags = (ULONG)map_info->Flag;
    if ((flags & EVENTMAP_INFO_FLAG_MANIFEST_PATTERNMAP) != 0U) {
        map_string = get_map_string(
            map_info,
            map_size,
            map_info->FormatStringOffset,
            &string_length,
            0
        );
        wprintf(L"map_type=pattern_map message_only=true map_name=\"");
        print_escaped_map_string(map_name, name_length);
        wprintf(L"\"");
        if (map_string != NULL) {
            wprintf(L" pattern_format=\"");
            print_escaped_map_string(map_string, string_length);
            wprintf(L"\"");
        }
        for (index = 0U; index < map_info->EntryCount && index < 128U; ++index) {
            const wchar_t *pattern;
            const wchar_t *replacement;
            ULONG pattern_length;
            ULONG replacement_length;

            pattern = get_map_string(
                map_info,
                map_size,
                map_info->MapEntryArray[index].InputOffset,
                &pattern_length,
                0
            );
            replacement = get_map_string(
                map_info,
                map_size,
                map_info->MapEntryArray[index].OutputOffset,
                &replacement_length,
                0
            );
            if (pattern == NULL || replacement == NULL) {
                continue;
            }

            wprintf(L" pattern_entry[%lu]=\"", (unsigned long)index);
            print_escaped_map_string(pattern, pattern_length);
            wprintf(L"\"=>\"");
            print_escaped_map_string(replacement, replacement_length);
            wprintf(L"\"");
        }
        if (map_info->EntryCount > 128U) {
            wprintf(L" pattern_entries_truncated=%lu", (unsigned long)(map_info->EntryCount - 128U));
        }
        wprintf(L" ");
        free(map_buffer);
        return 0;
    }

    is_value_map =
        (flags & EVENTMAP_INFO_FLAG_MANIFEST_VALUEMAP) != 0U ||
        ((flags & EVENTMAP_INFO_FLAG_WBEM_VALUEMAP) != 0U &&
         (flags & ~(ULONG)EVENTMAP_INFO_FLAG_WBEM_VALUEMAP) != EVENTMAP_INFO_FLAG_WBEM_FLAG);
    is_bitmap =
        (flags & EVENTMAP_INFO_FLAG_MANIFEST_BITMAP) != 0U ||
        (flags & EVENTMAP_INFO_FLAG_WBEM_BITMAP) != 0U ||
        ((flags & EVENTMAP_INFO_FLAG_WBEM_VALUEMAP) != 0U &&
         (flags & ~(ULONG)EVENTMAP_INFO_FLAG_WBEM_VALUEMAP) == EVENTMAP_INFO_FLAG_WBEM_FLAG);

    if ((!is_value_map && !is_bitmap) ||
        (map_info->MapEntryValueType != EVENTMAP_ENTRY_VALUETYPE_ULONG &&
         map_info->MapEntryValueType != EVENTMAP_ENTRY_VALUETYPE_STRING) ||
        (is_bitmap &&
         map_info->MapEntryValueType != EVENTMAP_ENTRY_VALUETYPE_ULONG)) {
        free(map_buffer);
        return 0;
    }

    wprintf(L"map_type=%ls map_name=\"", is_bitmap ? L"bitmap" : L"value_map");
    print_escaped_map_string(map_name, name_length);
    wprintf(L"\" ");

    if (is_value_map &&
        (flags & EVENTMAP_INFO_FLAG_WBEM_NO_MAP) == 0U &&
        map_info->MapEntryValueType == EVENTMAP_ENTRY_VALUETYPE_STRING) {
        for (index = 0U; index < map_info->EntryCount; ++index) {
            input_offset = map_info->MapEntryArray[index].InputOffset;
            output_offset = map_info->MapEntryArray[index].OutputOffset;
            map_string = get_map_string(map_info, map_size, input_offset, &map_value_length, 0);
            if (map_string == NULL) {
                continue;
            }
            if (!map_string_matches(
                    in_type,
                    out_type,
                    value,
                    value_size,
                    map_string,
                    map_value_length)) {
                continue;
            }
            map_string = get_map_string(map_info, map_size, output_offset, &string_length, (flags & (EVENTMAP_INFO_FLAG_MANIFEST_VALUEMAP | EVENTMAP_INFO_FLAG_MANIFEST_BITMAP)) != 0U);
            if (map_string == NULL) {
                continue;
            }
            wprintf(L"value=\"");
            print_escaped_map_string(map_string, string_length);
            matched = 1;
            break;
        }
    } else if (get_map_numeric_value(in_type, value, value_size, &numeric_value)) {
        if (is_value_map) {
            if ((flags & EVENTMAP_INFO_FLAG_WBEM_NO_MAP) != 0U) {
                if (numeric_value < map_info->EntryCount) {
                    output_offset = map_info->MapEntryArray[numeric_value].OutputOffset;
                    map_string = get_map_string(map_info, map_size, output_offset, &string_length, (flags & (EVENTMAP_INFO_FLAG_MANIFEST_VALUEMAP | EVENTMAP_INFO_FLAG_MANIFEST_BITMAP)) != 0U);
                    if (map_string != NULL) {
                        wprintf(L"value=\"");
                        print_escaped_map_string(map_string, string_length);
                        matched = 1;
                    }
                }
            } else {
                for (index = 0U; index < map_info->EntryCount; ++index) {
                    if (map_info->MapEntryArray[index].Value != numeric_value) {
                        continue;
                    }
                    output_offset = map_info->MapEntryArray[index].OutputOffset;
                    map_string = get_map_string(map_info, map_size, output_offset, &string_length, (flags & (EVENTMAP_INFO_FLAG_MANIFEST_VALUEMAP | EVENTMAP_INFO_FLAG_MANIFEST_BITMAP)) != 0U);
                    if (map_string == NULL) {
                        continue;
                    }
                    wprintf(L"value=\"");
                    print_escaped_map_string(map_string, string_length);
                    matched = 1;
                    break;
                }
            }
        } else if (is_bitmap) {
            wprintf(L"value=\"");
            for (index = 0U; index < map_info->EntryCount; ++index) {
                ULONG mask;
                if ((flags & EVENTMAP_INFO_FLAG_WBEM_NO_MAP) != 0U) {
                    if (index >= 32U) {
                        break;
                    }
                    mask = 1UL << index;
                } else {
                    mask = map_info->MapEntryArray[index].Value;
                }

                if ((numeric_value & mask) != mask ||
                    (mask == 0U && numeric_value != 0U)) {
                    continue;
                }

                output_offset = map_info->MapEntryArray[index].OutputOffset;
                map_string = get_map_string(map_info, map_size, output_offset, &string_length, (flags & (EVENTMAP_INFO_FLAG_MANIFEST_VALUEMAP | EVENTMAP_INFO_FLAG_MANIFEST_BITMAP)) != 0U);
                if (map_string == NULL) {
                    continue;
                }

                if (matched_count != 0U) {
                    wprintf(L" | ");
                }
                print_escaped_map_string(map_string, string_length);
                matched_bits |= mask;
                ++matched_count;
            }

            if (matched_bits != numeric_value) {
                if (matched_count != 0U) {
                    wprintf(L" | ");
                }
                wprintf(L"0x%lx", (unsigned long)(numeric_value & ~matched_bits));
            }
            matched = 1;
        }
    }

    if (matched) {
        wprintf(L"\" raw=hex:");
        for (index = 0U; index < value_size; ++index) {
            wprintf(L"%02x", (unsigned int)value[index]);
        }
        wprintf(L"\n");
    }

    free(map_buffer);
    return matched;
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

