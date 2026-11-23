#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ws2tcpip.h>
#include <evntcons.h>
#include <tdh.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <wchar.h>
#include <sddl.h>
#include "etl_property_print.h"
#include "etl_value.h"
#include "etl_decoder.h"
#include "etl_property_format.h"

#define COMTRACE_MAX_DISPLAYED_EVENT_PROPERTIES 64U

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

static void print_binary_value(const BYTE *value, ULONG value_size)
{
    ULONG offset;

    wprintf(L"value=hex:");
    for (offset = 0U; offset < value_size; ++offset) {
        wprintf(L"%02x", (unsigned int)value[offset]);
    }
    wprintf(L"\n");
}



static int print_sid_value(const BYTE *value, ULONG value_size)
{
    LPWSTR sid_text = NULL;
    ULONG required_size;

    if (value == NULL ||
        value_size < 8U ||
        value[1] > SID_MAX_SUB_AUTHORITIES) {
        return 0;
    }

    required_size = GetSidLengthRequired(value[1]);
    if (required_size > value_size ||
        !IsValidSid((PSID)value) ||
        !ConvertSidToStringSidW((PSID)value, &sid_text)) {
        return 0;
    }

    wprintf(L"value=%ls\n", sid_text);
    (void)LocalFree(sid_text);
    return 1;
}


static int read_event_property_count(
    const EVENT_RECORD *event_record,
    const EVENT_PROPERTY_INFO *properties,
    ULONG property_count,
    const BYTE *buffer,
    ULONG buffer_size,
    const EVENT_PROPERTY_INFO *property,
    const wchar_t *const *parent_names,
    const ULONG *parent_indices,
    const ULONG *parent_property_indices,
    ULONG parent_count,
    ULONG *count)
{
    return comtrace_etl_read_property_count(
        event_record,
        properties,
        property_count,
        buffer,
        buffer_size,
        property,
        parent_names,
        parent_indices,
        parent_property_indices,
        parent_count,
        count
    );
}

static int is_hex_output_type(USHORT out_type)
{
    return out_type == TDH_OUTTYPE_HEXINT8 ||
        out_type == TDH_OUTTYPE_HEXINT16 ||
        out_type == TDH_OUTTYPE_HEXINT32 ||
        out_type == TDH_OUTTYPE_HEXINT64 ||
        out_type == TDH_OUTTYPE_ERRORCODE ||
        out_type == TDH_OUTTYPE_WIN32ERROR ||
        out_type == TDH_OUTTYPE_HRESULT ||
        out_type == TDH_OUTTYPE_NTSTATUS ||
        out_type == TDH_OUTTYPE_CODE_POINTER;
}

static int format_binary_hex_text(
    const BYTE *value,
    ULONG value_size,
    wchar_t *text,
    size_t text_capacity
)
{
    static const wchar_t digits[] = L"0123456789abcdef";
    size_t index;

    if (value == NULL || text == NULL ||
        text_capacity < 5U ||
        (size_t)value_size > (text_capacity - 5U) / 2U) {
        return 0;
    }

    text[0] = L'h';
    text[1] = L'e';
    text[2] = L'x';
    text[3] = L':';

    for (index = 0U; index < (size_t)value_size; ++index) {
        text[4U + index * 2U] = digits[value[index] >> 4U];
        text[5U + index * 2U] = digits[value[index] & 0x0FU];
    }

    text[4U + (size_t)value_size * 2U] = L'\0';
    return 1;
}

static int format_socket_address(
    const BYTE *value,
    ULONG value_size,
    wchar_t *text,
    size_t text_capacity
)
{
    ADDRESS_FAMILY family;

    if (value == NULL || text == NULL ||
        text_capacity == 0U ||
        value_size < sizeof(family)) {
        return 0;
    }

    CopyMemory(&family, value, sizeof(family));

    if (family == AF_INET) {
        SOCKADDR_IN address;
        wchar_t address_text[INET_ADDRSTRLEN];

        if (value_size < sizeof(address)) {
            return 0;
        }

        CopyMemory(&address, value, sizeof(address));
        if (InetNtopW(
                AF_INET,
                &address.sin_addr,
                address_text,
                _countof(address_text)) == NULL) {
            return 0;
        }

        return _snwprintf_s(
            text,
            text_capacity,
            _TRUNCATE,
            L"%ls:%u",
            address_text,
            (unsigned int)ntohs(address.sin_port)
        ) >= 0;
    }

    if (family == AF_INET6) {
        SOCKADDR_IN6 address;
        wchar_t address_text[INET6_ADDRSTRLEN];

        if (value_size < sizeof(address)) {
            return 0;
        }

        CopyMemory(&address, value, sizeof(address));
        if (InetNtopW(
                AF_INET6,
                &address.sin6_addr,
                address_text,
                _countof(address_text)) == NULL) {
            return 0;
        }

        if (address.sin6_scope_id != 0U) {
            return _snwprintf_s(
                text,
                text_capacity,
                _TRUNCATE,
                L"[%ls%%%lu]:%u",
                address_text,
                (unsigned long)address.sin6_scope_id,
                (unsigned int)ntohs(address.sin6_port)
            ) >= 0;
        }

        return _snwprintf_s(
            text,
            text_capacity,
            _TRUNCATE,
            L"[%ls]:%u",
            address_text,
            (unsigned int)ntohs(address.sin6_port)
        ) >= 0;
    }

    return format_binary_hex_text(value, value_size, text, text_capacity);
}

static int is_utf8_string_output_type(USHORT out_type)
{
    return out_type == TDH_OUTTYPE_UTF8 ||
        out_type == TDH_OUTTYPE_XML ||
        out_type == TDH_OUTTYPE_JSON;
}

static int print_utf8_property_value(
    const BYTE *value,
    ULONG value_size
)
{
    size_t content_size;
    size_t displayed_size;
    size_t conversion_size;
    size_t offset;
    int terminated;
    int converted = 0;
    size_t attempt;
    wchar_t decoded[257];

    if (!comtrace_scan_string(
            value,
            (size_t)value_size,
            1U,
            256U,
            &content_size,
            &displayed_size,
            &terminated)) {
        return 0;
    }

    conversion_size = displayed_size;
    for (attempt = 0U; attempt <= 3U; ++attempt) {
        if (conversion_size == 0U) {
            converted = 0;
            break;
        }

        converted = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            (LPCCH)value,
            (int)conversion_size,
            decoded,
            (int)_countof(decoded) - 1
        );
        if (converted > 0) {
            break;
        }

        if (content_size == displayed_size || attempt == 3U) {
            return 0;
        }
        --conversion_size;
    }

    decoded[converted] = L'\0';
    wprintf(L"value=\"");

    for (offset = 0U; offset < (size_t)converted; ++offset) {
        wchar_t character = decoded[offset];

        switch (character) {
        case L'\\':
            wprintf(L"\\\\");
            break;
        case L'"':
            wprintf(L"\\\"");
            break;
        case L'\n':
            wprintf(L"\n");
            break;
        case L'\r':
            wprintf(L"\\r");
            break;
        case L'\t':
            wprintf(L"\\t");
            break;
        default:
            if ((unsigned int)character < 0x20U ||
                (unsigned int)character > 0x7EU) {
                wprintf(L"\\u%04x", (unsigned int)character);
            } else {
                wprintf(L"%lc", character);
            }
            break;
        }
    }

    if (conversion_size < content_size) {
        wprintf(
            L"...(+%lu bytes)",
            (unsigned long)(content_size - conversion_size)
        );
    }
    if (!terminated) {
        wprintf(L" [unterminated]");
    }

    wprintf(L"\"\n");
    return 1;
}


static int event_property_is_array(const EVENT_PROPERTY_INFO *property)
{
    if (property == NULL) {
        return 0;
    }

    return (property->Flags & (
        PropertyParamCount |
        PropertyParamFixedCount
    )) != 0U || property->count > 1U;
}

static void build_property_display_name(
    wchar_t *display_name,
    size_t display_name_capacity,
    const wchar_t *name,
    const wchar_t *const *parent_names,
    const ULONG *parent_indices,
    ULONG parent_count,
    ULONG array_index
)
{
    ULONG index;
    wchar_t part[32];

    if (display_name == NULL || display_name_capacity == 0U) {
        return;
    }

    display_name[0] = L'\0';
    if (name == NULL ||
        (parent_count != 0U &&
            (parent_names == NULL || parent_indices == NULL))) {
        return;
    }

    for (index = 0U; index < parent_count; ++index) {
        if (parent_names[index] == NULL) {
            return;
        }
        if (index != 0U) {
            wcsncat_s(display_name, display_name_capacity, L".", _TRUNCATE);
        }
        wcsncat_s(display_name, display_name_capacity, parent_names[index], _TRUNCATE);
        if (parent_indices[index] != 0xFFFFFFFFUL) {
            _snwprintf_s(part, _countof(part), _TRUNCATE, L"[%lu]", (unsigned long)parent_indices[index]);
            wcsncat_s(display_name, display_name_capacity, part, _TRUNCATE);
        }
    }

    if (parent_count != 0U) {
        wcsncat_s(display_name, display_name_capacity, L".", _TRUNCATE);
    }
    wcsncat_s(display_name, display_name_capacity, name, _TRUNCATE);
    if (array_index != 0xFFFFFFFFUL) {
        _snwprintf_s(part, _countof(part), _TRUNCATE, L"[%lu]", (unsigned long)array_index);
        wcsncat_s(display_name, display_name_capacity, part, _TRUNCATE);
    }
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
    const BYTE *metadata_buffer,
    ULONG metadata_size,
    ULONG pointer_size)
{
    COMTRACE_ETL_PROPERTY_VALUE decoded_value;
    COMTRACE_ETL_PROPERTY_READ_STATUS read_status;
    BYTE *value;
    ULONG value_size;
    ULONG flags = (ULONG)property->Flags;
    USHORT in_type = (flags & PropertyHasCustomSchema) != 0U
        ? property->customSchemaType.InType
        : property->nonStructType.InType;
    USHORT out_type = (flags & PropertyHasCustomSchema) != 0U
        ? property->customSchemaType.OutType
        : property->nonStructType.OutType;
    wchar_t display_name[256];

    ZeroMemory(&decoded_value, sizeof(decoded_value));
    build_property_display_name(
        display_name,
        _countof(display_name),
        name,
        parent_names,
        parent_indices,
        parent_count,
        array_index
    );

    if ((flags & PropertyStruct) != 0U) {
        wprintf(
            L"  property_value[%lu] name=%ls status=skipped reason=struct_property\n",
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
        case COMTRACE_ETL_PROPERTY_READ_SIZE_LIMIT:
            wprintf(L"  property_value[%lu] name=%ls status=unavailable reason=property_size_limit maximum=%lu\n", (unsigned long)index, display_name, (unsigned long)COMTRACE_ETL_MAX_PROPERTY_BYTES);
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

    if ((flags & PropertyHasCustomSchema) == 0U &&
        !comtrace_etl_normalize_property_payload(
            &in_type,
            &value,
            &value_size,
            pointer_size)) {
        wprintf(
            L"  property_value[%lu] name=%ls status=unavailable reason=invalid_payload_framing\n",
            (unsigned long)index,
            display_name
        );
        goto cleanup;
    }

    wprintf(
        L"  property_value[%lu] name=%ls ",
        (unsigned long)index,
        display_name
    );
    if ((flags & PropertyWBEMXmlFragment) != 0U) {
        wprintf(L"format=wbem_xml_fragment ");
    }
    if ((flags & PropertyHasCustomSchema) != 0U) {
        wprintf(L"format=custom_schema");
        comtrace_etl_print_custom_schema_metadata(metadata_buffer, metadata_size, property);
        wprintf(L" ");
    }
    if ((flags & PropertyHasTags) != 0U) {
        wprintf(L"tags=0x%07lX ", (unsigned long)property->Tags);
    }
    if ((flags & PropertyParamCount) != 0U) {
        wprintf(L"count_property_index=%u ", (unsigned int)property->countPropertyIndex);
    } else if (event_property_is_array(property)) {
        wprintf(L"fixed_count=%u ", (unsigned int)property->count);
    }
    if ((flags & PropertyParamLength) != 0U) {
        wprintf(L"length_property_index=%u ", (unsigned int)property->lengthPropertyIndex);
    } else if ((flags & PropertyParamFixedLength) != 0U) {
        wprintf(L"fixed_length=%u ", (unsigned int)property->length);
    }
    if ((flags & PropertyHasCustomSchema) != 0U) {
        print_binary_value(value, value_size);
        goto cleanup;
    }
    if (out_type == TDH_OUTTYPE_NOPRINT) {
        wprintf(L"status=skipped reason=out_type_noprint\n");
        goto cleanup;
    }

    if (property->nonStructType.MapNameOffset != 0U &&
        comtrace_etl_print_mapped_property_value(
            event_record,
            metadata_buffer,
            metadata_size,
            property,
            in_type,
            out_type,
            value,
            value_size,
            pointer_size
        )) {
        goto cleanup;
    }

    if (out_type == TDH_OUTTYPE_DATETIME ||
        out_type == TDH_OUTTYPE_DATETIME_UTC ||
        out_type == TDH_OUTTYPE_CULTURE_INSENSITIVE_DATETIME ||
        out_type == TDH_OUTTYPE_CIMDATETIME ||
        out_type == TDH_OUTTYPE_ETWTIME ||
        out_type == TDH_OUTTYPE_REDUCEDSTRING ||
        (in_type == TDH_INTYPE_BINARY &&
         out_type != TDH_OUTTYPE_NULL &&
         out_type != TDH_OUTTYPE_HEXBINARY &&
         out_type != TDH_OUTTYPE_IPV6 &&
         out_type != TDH_OUTTYPE_SOCKETADDRESS) ||
        (out_type == TDH_OUTTYPE_STRING &&
         in_type != TDH_INTYPE_UNICODESTRING &&
         in_type != TDH_INTYPE_ANSISTRING &&
         in_type != TDH_INTYPE_NONNULLTERMINATEDSTRING &&
         in_type != TDH_INTYPE_NONNULLTERMINATEDANSISTRING)) {
        if (comtrace_etl_print_tdh_formatted_property(
                metadata_buffer,
                metadata_size,
                property,
                in_type,
                out_type,
                value,
                value_size,
                pointer_size
            )) {
            goto cleanup;
        }
    }

    switch (in_type) {
    case TDH_INTYPE_RESERVED24:
        print_binary_value(value, value_size);
        goto cleanup;

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
                wprintf(L"\n");
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

        if (is_utf8_string_output_type(out_type)) {
            if (!print_utf8_property_value(value, value_size)) {
                wprintf(L"status=unavailable reason=invalid_utf8\n");
            }
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
                wprintf(L"\n");
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

    case TDH_INTYPE_ANSICHAR: {
        BYTE character;
        if (!comtrace_value_size_is_valid(COMTRACE_VALUE_UINT8, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            goto cleanup;
        }
        CopyMemory(&character, value, sizeof(character));
        if (character >= 0x20U && character <= 0x7EU) {
            wprintf(L"value='%c'\n", (int)character);
        } else {
            wprintf(L"value=\\x%02x\n", (unsigned int)character);
        }
        goto cleanup;
    }

    case TDH_INTYPE_UINT8: {
        BYTE number;
        if (!comtrace_value_size_is_valid(COMTRACE_VALUE_UINT8, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            goto cleanup;
        }
        CopyMemory(&number, value, sizeof(number));
        if (out_type == TDH_OUTTYPE_HEXINT8) {
            wprintf(L"value=0x%02X\n", (unsigned int)number);
        } else {
            wprintf(L"value=%u\n", (unsigned int)number);
        }
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

    case TDH_INTYPE_UNICODECHAR: {
        USHORT character;
        if (!comtrace_value_size_is_valid(COMTRACE_VALUE_UINT16, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            goto cleanup;
        }
        CopyMemory(&character, value, sizeof(character));
        wprintf(L"value=U+%04X\n", (unsigned int)character);
        goto cleanup;
    }

    case TDH_INTYPE_UINT16: {
        USHORT number;
        if (!comtrace_value_size_is_valid(COMTRACE_VALUE_UINT16, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            goto cleanup;
        }
        CopyMemory(&number, value, sizeof(number));
        if (out_type == TDH_OUTTYPE_HEXINT16) {
            wprintf(L"value=0x%04X\n", (unsigned int)number);
        } else if (out_type == TDH_OUTTYPE_PORT) {
            wprintf(L"value=%u\n", (unsigned int)ntohs(number));
        } else {
            wprintf(L"value=%u\n", (unsigned int)number);
        }
        goto cleanup;
    }

    case TDH_INTYPE_INT32: {
        LONG number;
        if (!comtrace_value_size_is_valid(COMTRACE_VALUE_INT32, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            goto cleanup;
        }
        CopyMemory(&number, value, sizeof(number));
        if (is_hex_output_type(out_type)) {
            ULONG bits;
            CopyMemory(&bits, value, sizeof(bits));
            wprintf(L"value=0x%08lX\n", bits);
        } else {
            wprintf(L"value=%ld\n", number);
        }
        goto cleanup;
    }

    case TDH_INTYPE_UINT32: {
        ULONG number;
        if (!comtrace_value_size_is_valid(COMTRACE_VALUE_UINT32, value_size, 0U)) {
            wprintf(L"status=unavailable reason=type_size_mismatch\n");
            goto cleanup;
        }
        if (out_type == TDH_OUTTYPE_IPV4) {
            wprintf(
                L"value=%u.%u.%u.%u\n",
                (unsigned int)value[0],
                (unsigned int)value[1],
                (unsigned int)value[2],
                (unsigned int)value[3]
            );
        } else if (is_hex_output_type(out_type)) {
            CopyMemory(&number, value, sizeof(number));
            wprintf(L"value=0x%08lX\n", number);
        } else {
            CopyMemory(&number, value, sizeof(number));
            wprintf(L"value=%lu\n", number);
        }
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
        if (out_type == TDH_OUTTYPE_HEXINT64 ||
            out_type == TDH_OUTTYPE_CODE_POINTER) {
            wprintf(L"value=0x%016llX\n", number);
        } else {
            wprintf(L"value=%llu\n", number);
        }
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
        if (out_type == TDH_OUTTYPE_IPV6) {
            wchar_t address[46];
            if (value_size != 16U ||
                InetNtopW(AF_INET6, value, address, _countof(address)) == NULL) {
                wprintf(L"status=unavailable reason=invalid_ipv6_address\n");
            } else {
                wprintf(L"value=%ls\n", address);
            }
        } else if (out_type == TDH_OUTTYPE_SOCKETADDRESS) {
            wchar_t address[1024];
            if (!format_socket_address(value, value_size, address, _countof(address))) {
                wprintf(L"status=unavailable reason=invalid_socket_address\n");
            } else {
                wprintf(L"value=%ls\n", address);
            }
        } else {
            print_binary_value(value, value_size);
        }
        goto cleanup;

    case TDH_INTYPE_SID:
        if (!print_sid_value(value, value_size)) {
            wprintf(L"status=unavailable reason=invalid_sid\n");
        }
        goto cleanup;

    default:
        if ((flags & PropertyHasCustomSchema) == 0U &&
            !comtrace_etl_print_tdh_formatted_property(
                metadata_buffer,
                metadata_size,
                property,
                in_type,
                out_type,
                value,
                value_size,
                pointer_size
            )) {
            print_binary_value(value, value_size);
        }
        goto cleanup;
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
    const ULONG *parent_property_indices,
    ULONG parent_count)
{
    ULONG flags = (ULONG)property->Flags;
    ULONG element_index;
    wchar_t display_name[256];

    build_property_display_name(
        display_name,
        _countof(display_name),
        name,
        parent_names,
        parent_indices,
        parent_count,
        0xFFFFFFFFUL
    );

    if (parent_count != 0U && (flags & PropertyStruct) == 0U) {
        USHORT input_type =
            (flags & PropertyHasCustomSchema) != 0U
                ? property->customSchemaType.InType
                : property->nonStructType.InType;
        USHORT output_type =
            (flags & PropertyHasCustomSchema) != 0U
                ? property->customSchemaType.OutType
                : property->nonStructType.OutType;

        wprintf(
            L"  property_type name=%ls in_type=%u in_type_name=%ls out_type=%u out_type_name=%ls\n",
            display_name,
            (unsigned int)input_type,
            comtrace_etl_input_type_name(input_type),
            (unsigned int)output_type,
            comtrace_etl_output_type_name(output_type)
        );
    }

    if ((flags & PropertyStruct) != 0U) {
        ULONG member_start = property->structType.StructStartIndex;
        ULONG member_count = property->structType.NumOfStructMembers;
        ULONG member_index;
        ULONG struct_count = 1U;
        ULONG struct_index;
        const wchar_t *nested_names[COMTRACE_ETL_MAX_PROPERTY_DEPTH];
        ULONG nested_indices[COMTRACE_ETL_MAX_PROPERTY_DEPTH];
        ULONG nested_property_indices[COMTRACE_ETL_MAX_PROPERTY_DEPTH];

        if (parent_count >= COMTRACE_ETL_MAX_PROPERTY_DEPTH) {
            wprintf(L"  property_value[%lu] name=%ls status=unavailable reason=struct_depth_limit\n", (unsigned long)index, display_name);
            return;
        }
        if (member_start > property_count || member_count > property_count - member_start) {
            wprintf(L"  property_value[%lu] name=%ls status=unavailable reason=invalid_struct_members\n", (unsigned long)index, name);
            return;
        }
        if ((flags & PropertyParamCount) != 0U) {
            if (!read_event_property_count(
                    event_record, properties, property_count, buffer, buffer_size,
                    property, parent_names, parent_indices, parent_property_indices,
                    parent_count, &struct_count)) {
                wprintf(L"  property_value[%lu] name=%ls status=unavailable reason=invalid_count_property\n", (unsigned long)index, name);
                return;
            }
        } else if (event_property_is_array(property)) {
            struct_count = (ULONG)property->count;
        }
        if (struct_count > COMTRACE_ETL_MAX_ARRAY_ELEMENTS) {
            wprintf(L"  property_value[%lu] name=%ls status=unavailable reason=struct_count_limit\n", (unsigned long)index, name);
            return;
        }
        wprintf(
            L"  property_value[%lu] name=%ls type=struct elements=%lu\n",
            (unsigned long)index,
            display_name,
            (unsigned long)struct_count
        );

        for (element_index = 0U; element_index < parent_count; ++element_index) {
            nested_names[element_index] = parent_names[element_index];
            nested_indices[element_index] = parent_indices[element_index];
            nested_property_indices[element_index] = parent_property_indices[element_index];
        }
        nested_names[parent_count] = name;
        nested_property_indices[parent_count] = (ULONG)(property - properties);

        for (struct_index = 0U; struct_index < struct_count; ++struct_index) {
            nested_indices[parent_count] =
                struct_count == 1U && !event_property_is_array(property)
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
                    nested_property_indices,
                    parent_count + 1U
                );
            }
        }
        return;
    }

    if ((flags & PropertyParamCount) != 0U) {
        ULONG count;
        if (!read_event_property_count(
                event_record, properties, property_count, buffer, buffer_size,
                property, parent_names, parent_indices, parent_property_indices,
                parent_count, &count)) {
            wprintf(L"  property_value[%lu] name=%ls status=unavailable reason=invalid_count_property\n", (unsigned long)index, name);
            return;
        }
        for (element_index = 0U; element_index < count; ++element_index) {
            print_event_property_value_element(
                event_record, property, name, parent_names, parent_indices,
                parent_count, index, element_index, buffer, buffer_size, pointer_size
            );
        }
        return;
    }

    if (event_property_is_array(property)) {
        if ((ULONG)property->count > COMTRACE_ETL_MAX_ARRAY_ELEMENTS) {
            wprintf(L"  property_value[%lu] name=%ls status=unavailable reason=fixed_array_count_limit maximum=%lu\n", (unsigned long)index, name, (unsigned long)COMTRACE_ETL_MAX_ARRAY_ELEMENTS);
            return;
        }
        for (element_index = 0U; element_index < (ULONG)property->count; ++element_index) {
            print_event_property_value_element(
                event_record, property, name, parent_names, parent_indices,
                parent_count, index, element_index, buffer, buffer_size, pointer_size
            );
        }
        return;
    }

    print_event_property_value_element(
        event_record, property, name, parent_names, parent_indices,
        parent_count, index, 0xFFFFFFFFUL, buffer, buffer_size, pointer_size
    );
}

void comtrace_etl_print_event_properties(const EVENT_RECORD *event_record, ULONG pointer_size)
{
    COMTRACE_ETL_EVENT_INFO decoded_info;
    COMTRACE_ETL_EVENT_INFO_STATUS info_status;
    TRACE_EVENT_INFO *event_info;
    ULONG property_count;
    ULONG top_level_property_count;
    ULONG buffer_size;
    BYTE *buffer;
    ULONG index;

    ZeroMemory(&decoded_info, sizeof(decoded_info));
    info_status = comtrace_etl_event_info_open(event_record, &decoded_info);
    if (info_status != COMTRACE_ETL_EVENT_INFO_SUCCESS) {
        if (info_status == COMTRACE_ETL_EVENT_INFO_ALLOCATION_FAILED) {
            wprintf(L"  payload_schema=unavailable reason=allocation_failed\n");
        } else if (info_status == COMTRACE_ETL_EVENT_INFO_INVALID_METADATA) {
            wprintf(L"  payload_schema=unavailable reason=invalid_metadata\n");
        } else if (info_status == COMTRACE_ETL_EVENT_INFO_SIZE_LIMIT) {
            wprintf(L"  payload_schema=unavailable reason=metadata_size_limit maximum=%lu\n", (unsigned long)COMTRACE_ETL_MAX_METADATA_BYTES);
        } else {
            wprintf(L"  payload_schema=unavailable status=%lu\n", (unsigned long)decoded_info.tdh_status);
        }
        return;
    }

    event_info = decoded_info.info;
    buffer = decoded_info.buffer;
    buffer_size = decoded_info.size;
    property_count = decoded_info.property_count;
    top_level_property_count = decoded_info.top_level_property_count;
    if (top_level_property_count > COMTRACE_MAX_DISPLAYED_EVENT_PROPERTIES) {
        top_level_property_count = COMTRACE_MAX_DISPLAYED_EVENT_PROPERTIES;
    }

    wprintf(L"  payload_properties=%lu\n", (unsigned long)top_level_property_count);
    if (event_info->Tags != 0U) {
        wprintf(L"  payload_event_tags=0x%07lX\n", (unsigned long)event_info->Tags);
    }

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
            USHORT input_type =
                (property->Flags & PropertyHasCustomSchema) != 0U
                    ? property->customSchemaType.InType
                    : property->nonStructType.InType;
            USHORT output_type =
                (property->Flags & PropertyHasCustomSchema) != 0U
                    ? property->customSchemaType.OutType
                    : property->nonStructType.OutType;

            wprintf(
                L"  property[%lu] name=%.*ls in_type=%u in_type_name=%ls out_type=%u out_type_name=%ls",
                (unsigned long)index,
                (int)name_length,
                name,
                (unsigned int)input_type,
                comtrace_etl_input_type_name(input_type),
                (unsigned int)output_type,
                comtrace_etl_output_type_name(output_type)
            );
            if ((property->Flags & PropertyWBEMXmlFragment) != 0U) {
                wprintf(L" format=wbem_xml_fragment");
            }
            if ((property->Flags & PropertyHasCustomSchema) != 0U) {
                wprintf(L" format=custom_schema");
            }
            if ((property->Flags & PropertyHasTags) != 0U) {
                wprintf(L" tags=0x%07lX", (unsigned long)property->Tags);
            }
            if ((property->Flags & PropertyParamCount) != 0U) {
                wprintf(L" count_property_index=%u", (unsigned int)property->countPropertyIndex);
            } else if (event_property_is_array(property)) {
                wprintf(L" fixed_count=%u", (unsigned int)property->count);
            }
            if ((property->Flags & PropertyParamLength) != 0U) {
                wprintf(L" length_property_index=%u", (unsigned int)property->lengthPropertyIndex);
            } else if ((property->Flags & PropertyParamFixedLength) != 0U) {
                wprintf(L" fixed_length=%u", (unsigned int)property->length);
            }
            wprintf(L"\n");
        }

        if (index < COMTRACE_MAX_DISPLAYED_EVENT_PROPERTIES) {
            print_event_property_value(
                event_record,
                property,
                name,
                index,
                pointer_size,
                event_info->EventPropertyInfoArray,
                property_count,
                buffer,
                buffer_size,
                NULL,
                NULL,
                NULL,
                0U
            );
        }
    }

    if (decoded_info.top_level_property_count > top_level_property_count) {
        wprintf(
            L"  payload_value_limit=%lu remaining=%lu\n",
            (unsigned long)COMTRACE_MAX_DISPLAYED_EVENT_PROPERTIES,
            (unsigned long)(decoded_info.top_level_property_count - top_level_property_count)
        );
    }

    comtrace_etl_event_info_close(&decoded_info);
}

