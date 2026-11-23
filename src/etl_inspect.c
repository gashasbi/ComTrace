#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <ws2tcpip.h>
#include <evntrace.h>
#include <evntcons.h>
#include <tdh.h>
#include <stddef.h>
#include <limits.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <wchar.h>
#include <sddl.h>
#include "etl_inspect.h"

#define COMTRACE_MAX_DISPLAYED_EVENT_PROPERTIES 64U
#include "etl_value.h"
#include "com_event.h"
#include "ole_event.h"
#include "event_record.h"
#include "event_catalog.h"
#include "com_identity.h"
#include "etl_decoder.h"
#include "etl_property_format.h"
#include "com_timeline.h"
#include "relationship_graph.h"
#include "ole_timeline.h"
#include "rpc_timeline.h"
#include "dde_event.h"
#include "dde_timeline.h"
#include "process_lifecycle.h"
#include "com_lifetime_analysis.h"

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
    COMTRACE_RPC_TIMELINE rpc_timeline;
    COMTRACE_RPC_TIMELINE_STATUS rpc_timeline_status;
    COMTRACE_DDE_TIMELINE dde_timeline;
    COMTRACE_DDE_TIMELINE_STATUS dde_timeline_status;
    COMTRACE_RELATIONSHIP_GRAPH relationship_graph;
    COMTRACE_GRAPH_STATUS graph_status;
    COMTRACE_PROCESS_LIFECYCLE_CATALOG process_lifecycle;
    ULONGLONG process_lifecycle_failures;
    COMTRACE_COM_LIFETIME_ANALYSIS lifetime_analysis;
    COMTRACE_LIFETIME_STATUS lifetime_status;
    ULONGLONG lifetime_decode_failures;
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
    wprintf(
        L"%lld",
        (long long)event_record->EventHeader.TimeStamp.QuadPart
    );
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
            L"  property_type name=%ls in_type=%u in_type_name=%ls out_type=%u out_type_name=%ls\\n",
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
            L"  property_value[%lu] name=%ls type=struct elements=%lu\\n",
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

static void print_event_properties(const EVENT_RECORD *event_record, ULONG pointer_size)
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

        while (name_length < remaining && name[name_length] != L'\\0') {
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
            wprintf(L"\\n");
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
            L"  payload_value_limit=%lu remaining=%lu\\n",
            (unsigned long)COMTRACE_MAX_DISPLAYED_EVENT_PROPERTIES,
            (unsigned long)(decoded_info.top_level_property_count - top_level_property_count)
        );
    }

    comtrace_etl_event_info_close(&decoded_info);
}

static BOOL get_tracelogging_event_name(
    const EVENT_RECORD *event_record,
    wchar_t *event_name,
    size_t event_name_capacity
)
{
    COMTRACE_ETL_EVENT_INFO decoded_info;
    COMTRACE_ETL_EVENT_INFO_STATUS info_status;
    TRACE_EVENT_INFO *event_info;
    ULONG name_offset;
    ULONG remaining;
    ULONG name_length = 0U;
    const wchar_t *name;
    BOOL result = FALSE;

    if (event_record == NULL ||
        event_name == NULL ||
        event_name_capacity < 2U) {
        return FALSE;
    }

    event_name[0] = L'\\0';
    ZeroMemory(&decoded_info, sizeof(decoded_info));
    info_status = comtrace_etl_event_info_open(event_record, &decoded_info);
    if (info_status != COMTRACE_ETL_EVENT_INFO_SUCCESS) {
        return FALSE;
    }

    event_info = decoded_info.info;
    name_offset = event_info->EventNameOffset;
    if (event_info->DecodingSource == DecodingSourceTlg &&
        name_offset != 0U &&
        name_offset < decoded_info.size &&
        (name_offset % (ULONG)sizeof(wchar_t)) == 0U) {
        name = (const wchar_t *)(decoded_info.buffer + name_offset);
        remaining = (decoded_info.size - name_offset) / (ULONG)sizeof(wchar_t);

        while (name_length < remaining && name[name_length] != L'\\0') {
            ++name_length;
        }

        if (name_length != 0U &&
            name_length < remaining &&
            (size_t)name_length < event_name_capacity &&
            wcsncpy_s(event_name, event_name_capacity, name, name_length) == 0) {
            result = TRUE;
        }
    }

    comtrace_etl_event_info_close(&decoded_info);
    return result;
}

static void print_com_lifetime_event_classification(
    const EVENT_RECORD *event_record
)
{
    wchar_t trace_event_name[128];
    const wchar_t *event_name = NULL;
    ULONGLONG object_instance_id = 0ULL;
    ULONG initial_reference_count = 0U;
    ULONG reference_count = 0U;
    GUID value;

    if (event_record == NULL ||
        !get_tracelogging_event_name(
            event_record,
            trace_event_name,
            _countof(trace_event_name)
        ) ||
        !comtrace_classify_com_lifetime_event(
            &event_record->EventHeader.ProviderId,
            trace_event_name,
            &event_name
        )) {
        return;
    }

    wprintf(L"  com_lifetime_event=%ls\n", event_name);

    if (comtrace_extract_guid_property(
            event_record,
            L"InstrumentationId",
            &value
        ) == COMTRACE_IDENTITY_SUCCESS) {
        GUID empty_instrumentation_id = {0};
        if (memcmp(&value, &empty_instrumentation_id, sizeof(GUID)) != 0) {
            wprintf(L"  com_lifetime_instrumentation_id=");
            print_guid(&value);
            wprintf(L"\\n");
        }
    }

    if (comtrace_extract_uint64_property(
            event_record,
            L"ObjectInstanceId",
            &object_instance_id
        ) == COMTRACE_IDENTITY_SUCCESS) {
        wprintf(L"  com_lifetime_object_instance_id=%llu\n",
            (unsigned long long)object_instance_id);
    }

    if (comtrace_extract_guid_property(
            event_record,
            L"ClassId",
            &value
        ) == COMTRACE_IDENTITY_SUCCESS) {
        wprintf(L"  com_lifetime_clsid=");
        print_guid(&value);
        wprintf(L"\n");
    }

    if (comtrace_extract_guid_property(
            event_record,
            L"InterfaceId",
            &value
        ) == COMTRACE_IDENTITY_SUCCESS) {
        wprintf(L"  com_lifetime_iid=");
        print_guid(&value);
        wprintf(L"\n");
    }

    if (comtrace_extract_uint32_property(
            event_record,
            L"InitialReferenceCount",
            &initial_reference_count
        ) == COMTRACE_IDENTITY_SUCCESS) {
        wprintf(L"  com_lifetime_initial_reference_count=%lu\n",
            (unsigned long)initial_reference_count);
    }

    if (comtrace_extract_uint32_property(
            event_record,
            L"ReferenceCountResult",
            &reference_count
        ) == COMTRACE_IDENTITY_SUCCESS) {
        wprintf(L"  com_lifetime_reference_count_result=%lu\n",
            (unsigned long)reference_count);
    }

    if (comtrace_extract_uint64_property(
            event_record,
            L"NativeObjectId",
            &object_instance_id
        ) == COMTRACE_IDENTITY_SUCCESS) {
        wprintf(L"  com_lifetime_native_object_id=0x%llx\n",
            (unsigned long long)object_instance_id);
    }

    if (comtrace_extract_guid_property(
            event_record,
            L"Ipid",
            &value
        ) == COMTRACE_IDENTITY_SUCCESS) {
        GUID empty_ipid = {0};
        if (memcmp(&value, &empty_ipid, sizeof(GUID)) != 0) {
            wprintf(L"  com_lifetime_ipid=");
            print_guid(&value);
            wprintf(L"\n");
        }
    }
}


static BOOL is_com_lifetime_event_record(
    const EVENT_RECORD *event_record
)
{
    wchar_t trace_event_name[128];
    const wchar_t *event_name = NULL;

    return event_record != NULL &&
        get_tracelogging_event_name(
            event_record,
            trace_event_name,
            _countof(trace_event_name)
        ) &&
        comtrace_classify_com_lifetime_event(
            &event_record->EventHeader.ProviderId,
            trace_event_name,
            &event_name
        );
}

static void add_com_lifetime_analysis_event(
    ETL_INSPECT_CONTEXT *context,
    const EVENT_RECORD *event_record
)
{
    wchar_t trace_event_name[128];
    const wchar_t *event_name = NULL;
    COMTRACE_LIFETIME_EVENT event;
    COMTRACE_LIFETIME_STATUS status;
    COMTRACE_ACTIVITY_IDS activity_ids;
    ULONGLONG process_generation = 0ULL;
    ULONG reference_count = 0U;

    if (context == NULL ||
        event_record == NULL ||
        !get_tracelogging_event_name(
            event_record,
            trace_event_name,
            _countof(trace_event_name)
        ) ||
        !comtrace_classify_com_lifetime_event(
            &event_record->EventHeader.ProviderId,
            trace_event_name,
            &event_name
        )) {
        return;
    }

    ZeroMemory(&event, sizeof(event));
    ZeroMemory(&activity_ids, sizeof(activity_ids));
    if (wcscmp(event_name, L"ComObjectCreated") == 0) {
        event.type = COMTRACE_LIFETIME_EVENT_CREATED;
    } else if (wcscmp(event_name, L"ComAddRef") == 0) {
        event.type = COMTRACE_LIFETIME_EVENT_ADD_REF;
    } else if (wcscmp(event_name, L"ComRelease") == 0) {
        event.type = COMTRACE_LIFETIME_EVENT_RELEASE;
    } else if (wcscmp(event_name, L"ComObjectDestroyed") == 0) {
        event.type = COMTRACE_LIFETIME_EVENT_DESTROYED;
    } else if (wcscmp(event_name, L"ComObjectNativeIdentityBound") == 0) {
        event.type = COMTRACE_LIFETIME_EVENT_NATIVE_IDENTITY_BOUND;
    } else {
        return;
    }

    event.process_id = event_record->EventHeader.ProcessId;
    event.thread_id = event_record->EventHeader.ThreadId;
    event.timestamp =
        (ULONGLONG)event_record->EventHeader.TimeStamp.QuadPart;

    if (comtrace_extract_guid_property(
            event_record,
            L"InstrumentationId",
            &event.instrumentation_id
        ) == COMTRACE_IDENTITY_SUCCESS) {
        GUID empty_instrumentation_id = {0};
        if (memcmp(&event.instrumentation_id,
                   &empty_instrumentation_id,
                   sizeof(GUID)) != 0) {
            event.has_instrumentation_id = TRUE;
        }
    }

    if (comtrace_extract_activity_ids(event_record, &activity_ids) ==
        COMTRACE_IDENTITY_SUCCESS) {
        event.has_activity_id = activity_ids.has_activity_id;
        event.activity_id = activity_ids.activity_id;
        event.has_related_activity_id = activity_ids.has_related_activity_id;
        event.related_activity_id = activity_ids.related_activity_id;
    }

    if (event.process_id == 0U ||
        comtrace_extract_uint64_property(
            event_record,
            L"ObjectInstanceId",
            &event.object_instance_id
        ) != COMTRACE_IDENTITY_SUCCESS ||
        event.object_instance_id == 0ULL) {
        if (context->lifetime_decode_failures != ULLONG_MAX) {
            ++context->lifetime_decode_failures;
        }
        return;
    }

    if (comtrace_process_lifecycle_resolve_generation(
            &context->process_lifecycle,
            event.process_id,
            (LONGLONG)event.timestamp,
            &process_generation
        )) {
        event.process_generation = process_generation;
        event.has_process_generation = TRUE;
    }

    if (comtrace_extract_guid_property(
            event_record,
            L"InterfaceId",
            &event.interface_id
        ) == COMTRACE_IDENTITY_SUCCESS) {
        event.has_interface_id = TRUE;
    }

    if (event.type == COMTRACE_LIFETIME_EVENT_CREATED &&
        comtrace_extract_guid_property(
            event_record,
            L"ClassId",
            &event.class_id
        ) == COMTRACE_IDENTITY_SUCCESS) {
        event.has_class_id = TRUE;
    }

    if (event.type == COMTRACE_LIFETIME_EVENT_NATIVE_IDENTITY_BOUND) {
        GUID empty_ipid = {0};

        if (comtrace_extract_uint64_property(
                event_record,
                L"NativeObjectId",
                &event.native_object_id
            ) != COMTRACE_IDENTITY_SUCCESS ||
            event.native_object_id == 0ULL ||
            !event.has_interface_id) {
            if (context->lifetime_decode_failures != ULLONG_MAX) {
                ++context->lifetime_decode_failures;
            }
            return;
        }
        event.has_native_object_id = TRUE;

        if (comtrace_extract_guid_property(
                event_record,
                L"Ipid",
                &event.native_ipid
            ) == COMTRACE_IDENTITY_SUCCESS &&
            memcmp(&event.native_ipid, &empty_ipid, sizeof(GUID)) != 0) {
            event.has_native_ipid = TRUE;
        }
    }

    if (event.type == COMTRACE_LIFETIME_EVENT_CREATED &&
        comtrace_extract_uint32_property(
            event_record,
            L"InitialReferenceCount",
            &event.initial_reference_count
        ) == COMTRACE_IDENTITY_SUCCESS) {
        event.has_initial_reference_count = TRUE;
    }

    if ((event.type == COMTRACE_LIFETIME_EVENT_ADD_REF ||
         event.type == COMTRACE_LIFETIME_EVENT_RELEASE) &&
        comtrace_extract_uint32_property(
            event_record,
            L"ReferenceCountResult",
            &reference_count
        ) == COMTRACE_IDENTITY_SUCCESS) {
        event.reference_count_result = reference_count;
        event.has_reference_count_result = TRUE;
    }

    status = comtrace_lifetime_analysis_add_event(
        &context->lifetime_analysis,
        &event,
        NULL
    );
    if (status != COMTRACE_LIFETIME_SUCCESS) {
        context->lifetime_status = status;
    }
}

static void add_lifetime_records_to_graph(
    ETL_INSPECT_CONTEXT *context
)
{
    size_t index;

    if (context == NULL) {
        return;
    }

    for (index = 0U;
         index < comtrace_lifetime_analysis_count(&context->lifetime_analysis);
         ++index) {
        const COMTRACE_LIFETIME_RECORD *record =
            comtrace_lifetime_analysis_get(&context->lifetime_analysis, index);

        if (record != NULL &&
            comtrace_relationship_graph_add_lifetime_record(
                &context->relationship_graph,
                record
            ) != COMTRACE_GRAPH_SUCCESS) {
            context->graph_status = COMTRACE_GRAPH_ALLOCATION_FAILED;
            return;
        }
    }
}

static void print_com_lifetime_summary(
    const ETL_INSPECT_CONTEXT *context
)
{
    size_t index;
    size_t count;
    ULONGLONG created_objects = 0ULL;
    ULONGLONG destroyed_objects = 0ULL;
    ULONGLONG incomplete_objects = 0ULL;
    ULONGLONG zero_release_without_destroy = 0ULL;

    if (context == NULL) {
        return;
    }

    count = comtrace_lifetime_analysis_count(&context->lifetime_analysis);

    for (index = 0U; index < count; ++index) {
        const COMTRACE_LIFETIME_RECORD *record =
            comtrace_lifetime_analysis_get(&context->lifetime_analysis, index);

        if (record == NULL) {
            continue;
        }

        if (record->has_created) {
            ++created_objects;
            if (!record->has_destroyed) {
                ++incomplete_objects;
            }
        }
        if (record->has_destroyed) {
            ++destroyed_objects;
        }
        if (record->release_zero_results != 0ULL && !record->has_destroyed) {
            ++zero_release_without_destroy;
        }
    }

    wprintf(L"\nCOM object lifetime timeline summary\n");
    wprintf(L"Objects tracked: %llu\n", (unsigned long long)count);
    wprintf(L"Objects created: %llu\n", (unsigned long long)created_objects);
    wprintf(L"Objects with explicit destruction: %llu\n",
        (unsigned long long)destroyed_objects);
    wprintf(L"Created objects without destruction event: %llu\n",
        (unsigned long long)incomplete_objects);
    wprintf(L"AddRef events: %llu\n",
        (unsigned long long)context->lifetime_analysis.add_ref_events);
    wprintf(L"Release events: %llu\n",
        (unsigned long long)context->lifetime_analysis.release_events);
    wprintf(L"Release-zero observations without destruction event: %llu\n",
        (unsigned long long)zero_release_without_destroy);
    wprintf(L"Events without creation: %llu\n",
        (unsigned long long)context->lifetime_analysis.events_without_creation);
    wprintf(L"Events after destruction: %llu\n",
        (unsigned long long)context->lifetime_analysis.events_after_destruction);
    wprintf(L"Duplicate creation events: %llu\n",
        (unsigned long long)context->lifetime_analysis.duplicate_creations);
    wprintf(L"Destructions without creation: %llu\n",
        (unsigned long long)context->lifetime_analysis.destructions_without_creation);
    wprintf(L"Duplicate destruction events: %llu\n",
        (unsigned long long)context->lifetime_analysis.duplicate_destructions);
    wprintf(L"Lifetime event decode failures: %llu\n",
        (unsigned long long)context->lifetime_decode_failures);

    for (index = 0U; index < count && index < 100U; ++index) {
        const COMTRACE_LIFETIME_RECORD *record =
            comtrace_lifetime_analysis_get(&context->lifetime_analysis, index);

        if (record == NULL) {
            continue;
        }

        wprintf(
            L"  object pid=%lu generation=",
            (unsigned long)record->process_id
        );
        if (record->has_process_generation) {
            wprintf(L"%llu", (unsigned long long)record->process_generation);
        } else {
            wprintf(L"unknown");
        }
        if (record->has_instrumentation_id) {
            wprintf(L" instrumentation_id=");
            print_guid(&record->instrumentation_id);
        }
        wprintf(L" instance=0x%llx observations=%llu create=%ls destroy=%ls",
            (unsigned long long)record->object_instance_id,
            (unsigned long long)record->observation_count,
            record->has_created ? L"observed" : L"missing",
            record->has_destroyed ? L"observed" : L"missing");

        if (record->has_created) {
            wprintf(L" created_ticks=%llu",
                (unsigned long long)record->created_timestamp);
        }
        if (record->has_destroyed) {
            wprintf(L" destroyed_ticks=%llu",
                (unsigned long long)record->destroyed_timestamp);
            if (record->has_created &&
                record->destroyed_timestamp >= record->created_timestamp) {
                ULONGLONG duration =
                    record->destroyed_timestamp - record->created_timestamp;
                if (context->trace_frequency != 0ULL) {
                    double duration_ms =
                        1000.0 * (double)duration /
                        (double)context->trace_frequency;
                    wprintf(L" lifetime_ms=%.3f", duration_ms);
                } else {
                    wprintf(L" lifetime_ticks=%llu",
                        (unsigned long long)duration);
                }
            }
        }

        if (record->has_class_id) {
            wprintf(L" clsid=");
            print_guid(&record->class_id);
        }
        if (record->interface_id_count != 0U) {
            size_t interface_position;
            wprintf(L" iids=");
            for (interface_position = 0U;
                 interface_position < record->interface_id_count;
                 ++interface_position) {
                if (interface_position != 0U) {
                    wprintf(L",");
                }
                print_guid(&record->interface_ids[interface_position].interface_id);
            }
        } else if (record->has_interface_id) {
            wprintf(L" iid=");
            print_guid(&record->interface_id);
        }
        if (record->activity_id_count != 0U) {
            size_t activity_position;
            size_t activity_display_count = record->activity_id_count > 2U
                ? 2U
                : record->activity_id_count;
            wprintf(L" activity_bindings=%llu",
                (unsigned long long)record->activity_id_count);
            for (activity_position = 0U;
                 activity_position < activity_display_count;
                 ++activity_position) {
                const COMTRACE_LIFETIME_ACTIVITY_ID *activity =
                    &record->activity_ids[activity_position];
                if (activity->has_activity_id) {
                    wprintf(L" activity_id=");
                    print_guid(&activity->activity_id);
                }
                if (activity->has_related_activity_id) {
                    wprintf(L" related_activity_id=");
                    print_guid(&activity->related_activity_id);
                }
            }
        }
        if (record->has_native_object_id) {
            wprintf(L" native_oid=0x%llx",
                (unsigned long long)record->native_object_id);
        }
        if (record->has_native_ipid) {
            wprintf(L" native_ipid=");
            print_guid(&record->native_ipid);
        }
        if (record->native_identity_count > 1U) {
            wprintf(L" native_identity_bindings=%llu",
                (unsigned long long)record->native_identity_count);
        }
        if (record->has_initial_reference_count) {
            wprintf(L" initial_reference_count=%lu",
                (unsigned long)record->initial_reference_count);
        }
        if (record->has_last_reference_count_result) {
            wprintf(L" last_reference_count_result=%lu",
                (unsigned long)record->last_reference_count_result);
        }
        if (record->release_zero_results != 0ULL) {
            wprintf(L" release_zero_observations=%llu",
                (unsigned long long)record->release_zero_results);
        }
        wprintf(L"\n");
    }

    if (count > 100U) {
        wprintf(L"  lifetime_display_limit=100 remaining=%llu\n",
            (unsigned long long)(count - 100U));
    }
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

static void print_com_rundown_event_classification(
    const EVENT_RECORD *event_record
)
{
    const wchar_t *event_name = NULL;
    COMTRACE_COM_OBJECT_IDENTITY identity;

    if (event_record == NULL ||
        !comtrace_classify_com_rundown_event(
            &event_record->EventHeader.ProviderId,
            event_record->EventHeader.EventDescriptor.Id,
            event_record->EventHeader.EventDescriptor.Version,
            event_record->EventHeader.EventDescriptor.Opcode,
            &event_name
        )) {
        return;
    }

    wprintf(L"  com_rundown_event=%ls\n", event_name);

    if (comtrace_extract_com_object_identity(event_record, &identity) !=
        COMTRACE_IDENTITY_SUCCESS) {
        return;
    }

    if (identity.has_oid) {
        wprintf(L"  com_rundown_oid=0x%llx\n", (unsigned long long)identity.oid);
    }
    if (identity.has_iid) {
        wprintf(L"  com_rundown_iid=");
        print_guid(&identity.iid);
        wprintf(L"\n");
    }
    if (identity.has_ipid) {
        wprintf(L"  com_rundown_ipid=");
        print_guid(&identity.ipid);
        wprintf(L"\n");
    }
}


static void print_com_runtime_event_classification(
    const EVENT_RECORD *event_record
)
{
    const wchar_t *event_name = NULL;
    COMTRACE_COM_OBJECT_IDENTITY identity;

    if (event_record == NULL ||
        !comtrace_classify_com_runtime_event(
            &event_record->EventHeader.ProviderId,
            event_record->EventHeader.EventDescriptor.Id,
            event_record->EventHeader.EventDescriptor.Version,
            event_record->EventHeader.EventDescriptor.Opcode,
            &event_name
        )) {
        return;
    }

    wprintf(L"  com_runtime_event=%ls\n", event_name);

    if (comtrace_extract_com_object_identity(event_record, &identity) !=
        COMTRACE_IDENTITY_SUCCESS) {
        return;
    }

    if (identity.has_iid) {
        wprintf(L"  com_stub_iid=");
        print_guid(&identity.iid);
        wprintf(L"\n");
    }
    if (identity.has_ipid) {
        wprintf(L"  com_stub_ipid=");
        print_guid(&identity.ipid);
        wprintf(L"\n");
    }
}

static void print_rpc_event_classification(
    const EVENT_RECORD *event_record
)
{
    const wchar_t *event_name = NULL;
    const wchar_t *phase = NULL;
    GUID interface_id;

    if (event_record == NULL ||
        !comtrace_classify_rpc_event(
            &event_record->EventHeader.ProviderId,
            event_record->EventHeader.EventDescriptor.Id,
            event_record->EventHeader.EventDescriptor.Version,
            event_record->EventHeader.EventDescriptor.Opcode,
            &event_name,
            &phase
        )) {
        return;
    }

    wprintf(L"  rpc_event=%ls phase=%ls\n", event_name, phase);

    if (comtrace_extract_guid_property(
            event_record,
            L"InterfaceUuid",
            &interface_id
        ) == COMTRACE_IDENTITY_SUCCESS) {
        wprintf(L"  rpc_interface_uuid=");
        print_guid(&interface_id);
        wprintf(L"\n");
    }
}

static void print_rpcss_event_classification(
    const EVENT_RECORD *event_record
)
{
    const wchar_t *event_name = NULL;
    GUID interface_id;
    GUID object_id;

    if (event_record == NULL ||
        !comtrace_classify_rpcss_event(
            &event_record->EventHeader.ProviderId,
            event_record->EventHeader.EventDescriptor.Id,
            event_record->EventHeader.EventDescriptor.Version,
            event_record->EventHeader.EventDescriptor.Opcode,
            &event_name
        )) {
        return;
    }

    wprintf(L"  rpcss_event=%ls\n", event_name);

    if (comtrace_extract_guid_property(
            event_record,
            L"InterfaceUUID",
            &interface_id
        ) == COMTRACE_IDENTITY_SUCCESS) {
        wprintf(L"  rpcss_interface_uuid=");
        print_guid(&interface_id);
        wprintf(L"\n");
    }

    if (comtrace_extract_guid_property(
            event_record,
            L"ObjectUUID",
            &object_id
        ) == COMTRACE_IDENTITY_SUCCESS) {
        wprintf(L"  rpcss_object_uuid=");
        print_guid(&object_id);
        wprintf(L"\n");
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

static void add_rpc_timeline_event(
    ETL_INSPECT_CONTEXT *context,
    const EVENT_RECORD *event_record
)
{
    const wchar_t *operation = NULL;
    const wchar_t *phase = NULL;
    COMTRACE_ACTIVITY_IDS activity_ids;
    COMTRACE_RPC_TIMELINE_EVENT timeline_event;

    if (context == NULL ||
        event_record == NULL ||
        !comtrace_classify_rpc_event(
            &event_record->EventHeader.ProviderId,
            event_record->EventHeader.EventDescriptor.Id,
            event_record->EventHeader.EventDescriptor.Version,
            event_record->EventHeader.EventDescriptor.Opcode,
            &operation,
            &phase)) {
        return;
    }

    if (wcscmp(phase, L"start") != 0 &&
        wcscmp(phase, L"stop") != 0) {
        return;
    }

    ZeroMemory(&timeline_event, sizeof(timeline_event));
    ZeroMemory(&activity_ids, sizeof(activity_ids));
    timeline_event.operation = operation;
    timeline_event.phase = wcscmp(phase, L"start") == 0
        ? COMTRACE_RPC_TIMELINE_PHASE_START
        : COMTRACE_RPC_TIMELINE_PHASE_STOP;
    timeline_event.process_id = event_record->EventHeader.ProcessId;
    timeline_event.thread_id = event_record->EventHeader.ThreadId;
    timeline_event.timestamp =
        (ULONGLONG)event_record->EventHeader.TimeStamp.QuadPart;

    if (comtrace_extract_activity_ids(event_record, &activity_ids) ==
        COMTRACE_IDENTITY_SUCCESS) {
        timeline_event.activity_ids = &activity_ids;
    }
    if (comtrace_extract_guid_property(
            event_record,
            L"InterfaceUuid",
            &timeline_event.interface_uuid
        ) == COMTRACE_IDENTITY_SUCCESS) {
        timeline_event.has_interface_uuid = TRUE;
    }
    if (comtrace_extract_uint32_property(
            event_record,
            L"ProcNum",
            &timeline_event.procedure_number
        ) == COMTRACE_IDENTITY_SUCCESS) {
        timeline_event.has_procedure_number = TRUE;
    }

    if (comtrace_rpc_timeline_add_event(
            &context->rpc_timeline,
            &timeline_event
        ) != COMTRACE_RPC_TIMELINE_SUCCESS) {
        context->rpc_timeline_status = COMTRACE_RPC_TIMELINE_ALLOCATION_FAILED;
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

static const wchar_t *rpc_timeline_match_name(
    COMTRACE_RPC_TIMELINE_MATCH_TYPE match_type
)
{
    switch (match_type) {
    case COMTRACE_RPC_TIMELINE_MATCH_ACTIVITY_ID:
        return L"ActivityId";
    case COMTRACE_RPC_TIMELINE_MATCH_PROCESS_THREAD:
        return L"process_thread_operation";
    default:
        return L"none";
    }
}

static void print_rpc_timeline_summary(
    const ETL_INSPECT_CONTEXT *context
)
{
    size_t count;
    size_t index;
    ULONGLONG matched = 0ULL;
    ULONGLONG matched_by_activity_id = 0ULL;
    ULONGLONG matched_by_process_thread = 0ULL;
    ULONGLONG incomplete_starts = 0ULL;

    if (context == NULL) {
        return;
    }

    wprintf(L"\\nRPC call timeline summary\\n");
    if (context->rpc_timeline_status != COMTRACE_RPC_TIMELINE_SUCCESS) {
        wprintf(L"  status=unavailable reason=allocation_failed\\n");
        return;
    }

    count = comtrace_rpc_timeline_count(&context->rpc_timeline);
    for (index = 0U; index < count; ++index) {
        const COMTRACE_RPC_TIMELINE_RECORD *record =
            comtrace_rpc_timeline_get(&context->rpc_timeline, index);

        if (record == NULL) {
            continue;
        }
        if (record->has_start && record->has_stop) {
            ++matched;
            if (record->match_type == COMTRACE_RPC_TIMELINE_MATCH_ACTIVITY_ID) {
                ++matched_by_activity_id;
            } else if (record->match_type == COMTRACE_RPC_TIMELINE_MATCH_PROCESS_THREAD) {
                ++matched_by_process_thread;
            }
        } else if (record->has_start) {
            ++incomplete_starts;
        }
    }

    wprintf(L"  RPC timeline records: %llu\\n", (unsigned long long)count);
    wprintf(L"  Matched RPC calls: %llu\\n", (unsigned long long)matched);
    wprintf(L"  Matched by ActivityId: %llu\\n", (unsigned long long)matched_by_activity_id);
    wprintf(L"  Matched by process/thread/operation: %llu\\n", (unsigned long long)matched_by_process_thread);
    wprintf(L"  Incomplete RPC starts: %llu\\n", (unsigned long long)incomplete_starts);
    wprintf(L"  Unmatched RPC stops: %llu\\n", (unsigned long long)context->rpc_timeline.unmatched_stops);
    wprintf(L"  RPC events without ActivityId: %llu\\n", (unsigned long long)context->rpc_timeline.missing_activity_ids);

    for (index = 0U; index < count && index < 100U; ++index) {
        const COMTRACE_RPC_TIMELINE_RECORD *record =
            comtrace_rpc_timeline_get(&context->rpc_timeline, index);

        if (record == NULL) {
            continue;
        }

        wprintf(
            L"  rpc_timeline operation=%ls pid=%lu tid=%lu",
            record->operation,
            (unsigned long)record->process_id,
            (unsigned long)record->thread_id
        );
        if (record->has_stop &&
            (record->stop_process_id != record->process_id ||
             record->stop_thread_id != record->thread_id)) {
            wprintf(
                L" stop_pid=%lu stop_tid=%lu",
                (unsigned long)record->stop_process_id,
                (unsigned long)record->stop_thread_id
            );
        }
        if (record->has_start) {
            wprintf(L" start_ticks=%llu", (unsigned long long)record->start_timestamp);
        }
        if (record->has_stop) {
            wprintf(L" stop_ticks=%llu", (unsigned long long)record->stop_timestamp);
        }
        if (record->has_duration) {
            wprintf(L" duration_ticks=%llu", (unsigned long long)record->duration_ticks);
        }
        if (record->has_interface_uuid) {
            wprintf(L" interface_uuid=");
            print_guid(&record->interface_uuid);
        }
        if (record->has_procedure_number) {
            wprintf(L" proc_num=%lu", (unsigned long)record->procedure_number);
        }
        if (record->has_activity_id) {
            wprintf(L" activity_id=");
            print_guid(&record->activity_id);
        }
        if (record->has_related_activity_id) {
            wprintf(L" related_activity_id=");
            print_guid(&record->related_activity_id);
        }
        wprintf(L" match=%ls\\n", rpc_timeline_match_name(record->match_type));
    }

    if (count > 100U) {
        wprintf(L"  timeline_display_limit=100 remaining=%llu\\n",
            (unsigned long long)(count - 100U));
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

static void add_dde_timeline_event(
    ETL_INSPECT_CONTEXT *context,
    const EVENT_RECORD *event_record
)
{
    wchar_t event_name[128];
    wchar_t role_name[32];
    wchar_t item[256];
    wchar_t service[256];
    wchar_t topic[256];
    COMTRACE_DDE_EVENT_TYPE event_type;
    COMTRACE_DDE_TIMELINE_EVENT timeline_event;
    BOOL boolean_value;

    if (context == NULL ||
        event_record == NULL ||
        !comtrace_is_dde_provider(&event_record->EventHeader.ProviderId) ||
        !get_tracelogging_event_name(
            event_record,
            event_name,
            sizeof(event_name) / sizeof(event_name[0])
        ) ||
        !comtrace_classify_dde_event(
            &event_record->EventHeader.ProviderId,
            event_name,
            &event_type
        )) {
        return;
    }

    ZeroMemory(&timeline_event, sizeof(timeline_event));
    ZeroMemory(role_name, sizeof(role_name));
    ZeroMemory(item, sizeof(item));
    ZeroMemory(service, sizeof(service));
    ZeroMemory(topic, sizeof(topic));

    if (comtrace_extract_uint64_property(
            event_record,
            L"ConversationId",
            &timeline_event.conversation_id
        ) != COMTRACE_IDENTITY_SUCCESS ||
        timeline_event.conversation_id == 0ULL) {
        return;
    }

    timeline_event.process_id = event_record->EventHeader.ProcessId;
    timeline_event.thread_id = event_record->EventHeader.ThreadId;
    timeline_event.timestamp =
        (ULONGLONG)event_record->EventHeader.TimeStamp.QuadPart;

    if (comtrace_extract_uint64_property(
            event_record,
            L"ConversationCorrelationId",
            &timeline_event.conversation_correlation_id
        ) != COMTRACE_IDENTITY_SUCCESS) {
        (void)comtrace_extract_uint64_property(
            event_record,
            L"CorrelationId",
            &timeline_event.conversation_correlation_id
        );
    }

    switch (event_type) {
    case COMTRACE_DDE_EVENT_CONVERSATION_OPENED:
        timeline_event.type = COMTRACE_DDE_TIMELINE_CONVERSATION_OPENED;
        (void)comtrace_extract_uint32_property(
            event_record,
            L"DdeInstanceId",
            &timeline_event.dde_instance_id
        );
        if (comtrace_extract_string_property(
                event_record,
                L"Role",
                role_name,
                sizeof(role_name) / sizeof(role_name[0])
            ) == COMTRACE_IDENTITY_SUCCESS &&
            wcscmp(role_name, L"client") == 0) {
            timeline_event.role = COMTRACE_DDE_ROLE_CLIENT;
        } else {
            timeline_event.role = COMTRACE_DDE_ROLE_SERVER;
        }
        (void)comtrace_extract_string_property(
            event_record,
            L"Service",
            service,
            sizeof(service) / sizeof(service[0])
        );
        (void)comtrace_extract_string_property(
            event_record,
            L"Topic",
            topic,
            sizeof(topic) / sizeof(topic[0])
        );
        timeline_event.service = service;
        timeline_event.topic = topic;
        break;

    case COMTRACE_DDE_EVENT_CONVERSATION_CLOSED:
        timeline_event.type = COMTRACE_DDE_TIMELINE_CONVERSATION_CLOSED;
        (void)comtrace_extract_uint32_property(
            event_record,
            L"Result",
            &timeline_event.result
        );
        break;

    case COMTRACE_DDE_EVENT_TRANSACTION_STARTED:
        timeline_event.type = COMTRACE_DDE_TIMELINE_TRANSACTION_STARTED;
        if (comtrace_extract_uint64_property(
                event_record,
                L"TransactionInstanceId",
                &timeline_event.transaction_instance_id
            ) != COMTRACE_IDENTITY_SUCCESS ||
            timeline_event.transaction_instance_id == 0ULL) {
            return;
        }
        (void)comtrace_extract_uint64_property(
            event_record,
            L"TransactionCorrelationId",
            &timeline_event.transaction_correlation_id
        );
        if (comtrace_extract_boolean_property(
                event_record,
                L"IsClient",
                &boolean_value
            ) != COMTRACE_IDENTITY_SUCCESS) {
            return;
        }
        timeline_event.role = boolean_value
            ? COMTRACE_DDE_ROLE_CLIENT
            : COMTRACE_DDE_ROLE_SERVER;
        (void)comtrace_extract_uint32_property(
            event_record,
            L"DdeTransactionId",
            &timeline_event.dde_transaction_id
        );
        (void)comtrace_extract_uint32_property(
            event_record,
            L"TransactionType",
            &timeline_event.transaction_type
        );
        (void)comtrace_extract_uint32_property(
            event_record,
            L"Format",
            &timeline_event.format
        );
        (void)comtrace_extract_boolean_property(
            event_record,
            L"Asynchronous",
            &timeline_event.asynchronous
        );
        (void)comtrace_extract_string_property(
            event_record,
            L"Item",
            item,
            sizeof(item) / sizeof(item[0])
        );
        timeline_event.item = item;
        break;

    case COMTRACE_DDE_EVENT_TRANSACTION_COMPLETED:
        timeline_event.type = COMTRACE_DDE_TIMELINE_TRANSACTION_COMPLETED;
        if (comtrace_extract_uint64_property(
                event_record,
                L"TransactionInstanceId",
                &timeline_event.transaction_instance_id
            ) != COMTRACE_IDENTITY_SUCCESS ||
            timeline_event.transaction_instance_id == 0ULL) {
            return;
        }
        (void)comtrace_extract_uint64_property(
            event_record,
            L"TransactionCorrelationId",
            &timeline_event.transaction_correlation_id
        );
        if (comtrace_extract_boolean_property(
                event_record,
                L"IsClient",
                &boolean_value
            ) == COMTRACE_IDENTITY_SUCCESS) {
            timeline_event.role = boolean_value
                ? COMTRACE_DDE_ROLE_CLIENT
                : COMTRACE_DDE_ROLE_SERVER;
        }
        (void)comtrace_extract_uint32_property(
            event_record,
            L"DdeTransactionId",
            &timeline_event.dde_transaction_id
        );
        (void)comtrace_extract_uint32_property(
            event_record,
            L"Result",
            &timeline_event.result
        );
        (void)comtrace_extract_uint32_property(
            event_record,
            L"DataSize",
            &timeline_event.data_size
        );
        (void)comtrace_extract_boolean_property(
            event_record,
            L"Succeeded",
            &timeline_event.succeeded
        );
        break;

    case COMTRACE_DDE_EVENT_CALLBACK_RECEIVED:
        timeline_event.type = COMTRACE_DDE_TIMELINE_CALLBACK_RECEIVED;
        (void)comtrace_extract_uint64_property(
            event_record,
            L"TransactionCorrelationId",
            &timeline_event.transaction_correlation_id
        );
        if (comtrace_extract_boolean_property(
                event_record,
                L"IsServerCallback",
                &boolean_value
            ) != COMTRACE_IDENTITY_SUCCESS) {
            return;
        }
        timeline_event.role = boolean_value
            ? COMTRACE_DDE_ROLE_SERVER
            : COMTRACE_DDE_ROLE_CLIENT;
        (void)comtrace_extract_uint32_property(
            event_record,
            L"TransactionType",
            &timeline_event.transaction_type
        );
        (void)comtrace_extract_uint32_property(
            event_record,
            L"Format",
            &timeline_event.format
        );
        (void)comtrace_extract_uint32_property(
            event_record,
            L"DataSize",
            &timeline_event.data_size
        );
        (void)comtrace_extract_string_property(
            event_record,
            L"Item",
            item,
            sizeof(item) / sizeof(item[0])
        );
        timeline_event.item = item;
        break;

    default:
        return;
    }

    if (comtrace_dde_timeline_add_event(
            &context->dde_timeline,
            &timeline_event
        ) != COMTRACE_DDE_TIMELINE_SUCCESS) {
        context->dde_timeline_status = COMTRACE_DDE_TIMELINE_ALLOCATION_FAILED;
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

        if (record->has_stop &&
            (record->stop_process_id != record->process_id ||
             record->stop_thread_id != record->thread_id)) {
            wprintf(
                L" stop_pid=%lu stop_tid=%lu",
                (unsigned long)record->stop_process_id,
                (unsigned long)record->stop_thread_id
            );
        }

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

static void print_dde_timeline_summary(
    const ETL_INSPECT_CONTEXT *context
)
{
    size_t count;
    size_t index;
    ULONGLONG conversations = 0ULL;
    ULONGLONG transactions = 0ULL;
    ULONGLONG callbacks = 0ULL;
    ULONGLONG completed = 0ULL;
    ULONGLONG incomplete = 0ULL;

    if (context == NULL) {
        return;
    }

    count = comtrace_dde_timeline_count(&context->dde_timeline);
    for (index = 0U; index < count; ++index) {
        const COMTRACE_DDE_TIMELINE_RECORD *record =
            comtrace_dde_timeline_get(&context->dde_timeline, index);

        if (record == NULL) {
            continue;
        }
        if (record->has_opened || record->has_closed) {
            ++conversations;
        }
        if (record->has_start) {
            ++transactions;
            if (record->has_completion) {
                ++completed;
            } else {
                ++incomplete;
            }
        }
        if (record->has_callback) {
            ++callbacks;
        }
    }

    wprintf(L"\nDDE timeline summary\n");
    wprintf(L"Conversation records: %llu\n", (unsigned long long)conversations);
    wprintf(L"Transactions started: %llu\n", (unsigned long long)transactions);
    wprintf(L"Transactions completed: %llu\n", (unsigned long long)completed);
    wprintf(L"Incomplete transactions: %llu\n", (unsigned long long)incomplete);
    wprintf(L"Server/client callbacks: %llu\n", (unsigned long long)callbacks);
    wprintf(
        L"Correlated client/server exchanges: %llu\n",
        (unsigned long long)comtrace_dde_timeline_exchange_count(&context->dde_timeline)
    );
    wprintf(
        L"Unmatched completions: %llu\n",
        (unsigned long long)context->dde_timeline.unmatched_completions
    );
    wprintf(
        L"Missing transaction correlation IDs: %llu\n",
        (unsigned long long)context->dde_timeline.missing_transaction_correlation_ids
    );
    wprintf(
        L"Ambiguous cross-process correlations: %llu\n",
        (unsigned long long)context->dde_timeline.ambiguous_correlations
    );

    for (index = 0U; index < count && index < 100U; ++index) {
        const COMTRACE_DDE_TIMELINE_RECORD *record =
            comtrace_dde_timeline_get(&context->dde_timeline, index);

        if (record == NULL) {
            continue;
        }

        wprintf(
            L"  pid=%lu tid=%lu conversation=%llu",
            (unsigned long)record->process_id,
            (unsigned long)record->thread_id,
            (unsigned long long)record->conversation_id
        );
        if (record->dde_instance_id != 0U) {
            wprintf(L" dde_instance=%lu", (unsigned long)record->dde_instance_id);
        }
        if (record->has_opened) {
            wprintf(L" opened=%llu", (unsigned long long)record->opened_timestamp);
        }
        if (record->has_closed) {
            wprintf(L" closed=%llu result=%lu",
                (unsigned long long)record->closed_timestamp,
                (unsigned long)record->close_result);
        }
        if (record->has_start) {
            wprintf(
                L" transaction=%llu start=%llu role=%ls type=0x%lx format=%lu item=%ls",
                (unsigned long long)record->transaction_instance_id,
                (unsigned long long)record->start_timestamp,
                record->role == COMTRACE_DDE_ROLE_CLIENT ? L"client" : L"server",
                (unsigned long)record->transaction_type,
                (unsigned long)record->format,
                record->item
            );
        }
        if (record->has_completion) {
            wprintf(
                L" completion=%llu succeeded=%ls duration_ticks=%llu",
                (unsigned long long)record->completion_timestamp,
                record->succeeded ? L"true" : L"false",
                (unsigned long long)record->duration_ticks
            );
        }
        if (record->has_callback) {
            wprintf(L" callback=%llu data_size=%lu",
                (unsigned long long)record->callback_timestamp,
                (unsigned long)record->data_size);
        }
        if (record->service[0] != L'\0' || record->topic[0] != L'\0') {
            wprintf(L" service=%ls topic=%ls", record->service, record->topic);
        }
        wprintf(L"\n");
    }

    if (count > 100U) {
        wprintf(L"  dde_timeline_display_limit=100 remaining=%llu\n",
            (unsigned long long)(count - 100U));
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
    case COMTRACE_GRAPH_EDGE_PROCESS_OWNS_LIFETIME_OBJECT:
        return L"process_owns_lifetime_object";
    case COMTRACE_GRAPH_EDGE_LIFETIME_OBJECT_HAS_CLSID:
        return L"lifetime_object_has_clsid";
    case COMTRACE_GRAPH_EDGE_LIFETIME_OBJECT_EXPOSES_IID:
        return L"lifetime_object_exposes_iid";
    case COMTRACE_GRAPH_EDGE_LIFETIME_OBJECT_HAS_ACTIVITY:
        return L"lifetime_object_has_activity";
    case COMTRACE_GRAPH_EDGE_LIFETIME_OBJECT_MAPS_TO_NATIVE_OBJECT:
        return L"lifetime_object_maps_to_native_object";
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
        if (node->has_process_generation) {
            wprintf(
                L" generation=%llu",
                (unsigned long long)node->process_generation
            );
        } else {
            wprintf(L" generation=unknown");
        }
        break;
    case COMTRACE_GRAPH_NODE_THREAD:
        wprintf(
            L"thread:%lu/%lu",
            (unsigned long)node->process_id,
            (unsigned long)node->thread_id
        );
        if (node->has_process_generation) {
            wprintf(
                L" generation=%llu",
                (unsigned long long)node->process_generation
            );
        } else {
            wprintf(L" generation=unknown");
        }
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
        if (node->has_process_generation) {
            wprintf(
                L" generation=%llu",
                (unsigned long long)node->process_generation
            );
        } else {
            wprintf(L" generation=unknown");
        }
        if (node->has_observed_timestamp) {
            wprintf(
                L" observed_ticks=%lld..%lld",
                (long long)node->first_seen_timestamp,
                (long long)node->last_seen_timestamp
            );
        }
        break;
    case COMTRACE_GRAPH_NODE_COM_LIFETIME_OBJECT:
        wprintf(
            L"com-lifetime-object:%lu/0x%llx observations=%llu",
            (unsigned long)node->process_id,
            (unsigned long long)node->value,
            (unsigned long long)node->observation_count
        );
        if (node->has_process_generation) {
            wprintf(L" generation=%llu",
                (unsigned long long)node->process_generation);
        } else {
            wprintf(L" generation=unknown");
        }
        if (node->has_observed_timestamp) {
            wprintf(L" observed_ticks=%lld..%lld",
                (long long)node->first_seen_timestamp,
                (long long)node->last_seen_timestamp);
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

static void add_process_lifecycle_event(
    ETL_INSPECT_CONTEXT *context,
    const EVENT_RECORD *event_record
)
{
    if (context == NULL ||
        event_record == NULL ||
        !comtrace_process_lifecycle_is_event(event_record)) {
        return;
    }

    {
        COMTRACE_PROCESS_EVENT process_event;
        COMTRACE_PROCESS_LIFECYCLE_STATUS process_status;

        process_status = comtrace_process_lifecycle_extract(
            event_record,
            &process_event
        );

        if (process_status == COMTRACE_PROCESS_LIFECYCLE_SUCCESS) {
            process_status = comtrace_process_lifecycle_catalog_add(
                &context->process_lifecycle,
                &process_event
            );

            if (process_status != COMTRACE_PROCESS_LIFECYCLE_SUCCESS) {
                context->graph_status = COMTRACE_GRAPH_ALLOCATION_FAILED;
            }
        } else if (context->process_lifecycle_failures != ULLONG_MAX) {
            ++context->process_lifecycle_failures;
        }
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
        identity.timestamp = event_record->EventHeader.TimeStamp.QuadPart;
        identity.has_process_id = identity.process_id != 0U;
        identity.has_thread_id = identity.thread_id != 0U;
        identity.has_timestamp = TRUE;
    }

    if (comtrace_extract_activity_ids(event_record, &activity_ids) ==
        COMTRACE_IDENTITY_SUCCESS) {
        activity_ids_ptr = &activity_ids;
    }

    if (comtrace_relationship_graph_add_event_with_lifecycle(
            &context->relationship_graph,
            &identity,
            activity_ids_ptr,
            &context->process_lifecycle
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
    size_t known_process_generations = 0U;
    size_t unknown_process_generations = 0U;

    if (graph == NULL) {
        return;
    }

    wprintf(L"\nRelationship graph summary\n");
    wprintf(
        L"Nodes: processes=%llu threads=%llu activities=%llu calls=%llu objects=%llu instrumented_objects=%llu clsids=%llu iids=%llu ipids=%llu\n",
        (unsigned long long)comtrace_relationship_graph_node_count(graph, COMTRACE_GRAPH_NODE_PROCESS),
        (unsigned long long)comtrace_relationship_graph_node_count(graph, COMTRACE_GRAPH_NODE_THREAD),
        (unsigned long long)comtrace_relationship_graph_node_count(graph, COMTRACE_GRAPH_NODE_ACTIVITY),
        (unsigned long long)comtrace_relationship_graph_node_count(graph, COMTRACE_GRAPH_NODE_CALL),
        (unsigned long long)comtrace_relationship_graph_node_count(graph, COMTRACE_GRAPH_NODE_COM_OBJECT),
        (unsigned long long)comtrace_relationship_graph_node_count(graph, COMTRACE_GRAPH_NODE_COM_LIFETIME_OBJECT),
        (unsigned long long)comtrace_relationship_graph_node_count(graph, COMTRACE_GRAPH_NODE_CLSID),
        (unsigned long long)comtrace_relationship_graph_node_count(graph, COMTRACE_GRAPH_NODE_IID),
        (unsigned long long)comtrace_relationship_graph_node_count(graph, COMTRACE_GRAPH_NODE_IPID)
    );
    wprintf(
        L"Edges: process_calls_process=%llu thread_initiates_call=%llu call_targets_thread=%llu call_uses_object=%llu related_activity=%llu object_ipid=%llu ipid_iid=%llu lifetime_objects_owned=%llu lifetime_object_activity=%llu\n",
        (unsigned long long)comtrace_relationship_graph_edge_count(graph, COMTRACE_GRAPH_EDGE_PROCESS_CALLS_PROCESS),
        (unsigned long long)comtrace_relationship_graph_edge_count(graph, COMTRACE_GRAPH_EDGE_THREAD_INITIATES_CALL),
        (unsigned long long)comtrace_relationship_graph_edge_count(graph, COMTRACE_GRAPH_EDGE_CALL_TARGETS_THREAD),
        (unsigned long long)comtrace_relationship_graph_edge_count(graph, COMTRACE_GRAPH_EDGE_CALL_USES_OBJECT),
        (unsigned long long)comtrace_relationship_graph_edge_count(graph, COMTRACE_GRAPH_EDGE_ACTIVITY_RELATED_TO),
        (unsigned long long)comtrace_relationship_graph_edge_count(graph, COMTRACE_GRAPH_EDGE_OBJECT_HAS_IPID),
        (unsigned long long)comtrace_relationship_graph_edge_count(graph, COMTRACE_GRAPH_EDGE_IPID_SUPPORTS_IID),
        (unsigned long long)comtrace_relationship_graph_edge_count(graph, COMTRACE_GRAPH_EDGE_PROCESS_OWNS_LIFETIME_OBJECT),
        (unsigned long long)comtrace_relationship_graph_edge_count(graph, COMTRACE_GRAPH_EDGE_LIFETIME_OBJECT_HAS_ACTIVITY)
    );

    for (index = 0U; index < graph->node_count; ++index) {
        const COMTRACE_GRAPH_NODE *node = &graph->nodes[index];

        if (node->type != COMTRACE_GRAPH_NODE_PROCESS) {
            continue;
        }

        if (node->has_process_generation) {
            ++known_process_generations;
        } else {
            ++unknown_process_generations;
        }
    }

    wprintf(
        L"Process node generations: known=%llu unknown=%llu\n",
        (unsigned long long)known_process_generations,
        (unsigned long long)unknown_process_generations
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

static void print_process_lifecycle_summary(
    const COMTRACE_PROCESS_LIFECYCLE_CATALOG *catalog,
    ULONGLONG decode_failures
)
{
    size_t index;
    size_t displayed;

    if (catalog == NULL) {
        return;
    }

    wprintf(L"\nProcess lifecycle summary\n");
    wprintf(
        L"Process lifecycle events: start=%llu end=%llu dc_start=%llu dc_end=%llu defunct=%llu decode_failures=%llu\n",
        (unsigned long long)catalog->start_events,
        (unsigned long long)catalog->end_events,
        (unsigned long long)catalog->dc_start_events,
        (unsigned long long)catalog->dc_end_events,
        (unsigned long long)catalog->defunct_events,
        (unsigned long long)decode_failures
    );
    wprintf(
        L"Observed process generations: %llu\n",
        (unsigned long long)catalog->count
    );

    displayed = catalog->count > 50U ? 50U : catalog->count;
    for (index = 0U; index < displayed; ++index) {
        const COMTRACE_PROCESS_LIFETIME *lifetime =
            &catalog->items[index];

        wprintf(
            L"  process=%lu generation=%llu",
            (unsigned long)lifetime->process_id,
            (unsigned long long)lifetime->generation
        );

        if (lifetime->has_parent_process_id) {
            wprintf(L" parent=%lu", (unsigned long)lifetime->parent_process_id);
        }

        if (lifetime->started_during_trace) {
            wprintf(L" start_ticks=%lld", (long long)lifetime->first_seen_timestamp);
        } else if (lifetime->has_first_seen_timestamp) {
            wprintf(L" start_kind=rundown first_seen_ticks=%lld", (long long)lifetime->first_seen_timestamp);
        } else {
            wprintf(L" start_kind=unknown");
        }

        if (lifetime->has_end_timestamp) {
            wprintf(L" end_ticks=%lld", (long long)lifetime->end_timestamp);
        } else {
            wprintf(L" end_ticks=unknown");
        }

        if (lifetime->has_trace_end_timestamp) {
            wprintf(
                L" running_at_trace_end=true trace_end_ticks=%lld",
                (long long)lifetime->trace_end_timestamp
            );
        }

        wprintf(L"\n");
    }

    if (catalog->count > displayed) {
        wprintf(
            L"  process_generation_display_limit=50 remaining=%llu\n",
            (unsigned long long)(catalog->count - displayed)
        );
    }
}

static int compare_search_numbers(
    USHORT in_type,
    USHORT out_type,
    const wchar_t *actual,
    const wchar_t *expected,
    int *comparison
)
{
    wchar_t *actual_end = NULL;
    wchar_t *expected_end = NULL;

    if (actual == NULL || expected == NULL || comparison == NULL ||
        actual[0] == L'\0' || expected[0] == L'\0') {
        return 0;
    }

    switch (in_type) {
    case TDH_INTYPE_INT8:
    case TDH_INTYPE_INT16:
    case TDH_INTYPE_INT64: {
        long long actual_value;
        long long expected_value;

        errno = 0;
        actual_value = wcstoll(actual, &actual_end, 10);
        if (errno == ERANGE || actual_end == actual ||
            actual_end == NULL || *actual_end != L'\0') {
            return 0;
        }

        errno = 0;
        expected_value = wcstoll(expected, &expected_end, 10);
        if (errno == ERANGE || expected_end == expected ||
            expected_end == NULL || *expected_end != L'\0') {
            return 0;
        }

        *comparison = actual_value < expected_value ? -1 :
            (actual_value > expected_value ? 1 : 0);
        return 1;
    }
    case TDH_INTYPE_INT32: {
        if (is_hex_output_type(out_type)) {
            unsigned long long actual_value;
            unsigned long long expected_value;

            errno = 0;
            actual_value = wcstoull(actual, &actual_end, 16);
            if (errno == ERANGE || actual_end == actual ||
                actual_end == NULL || *actual_end != L'\0') {
                return 0;
            }

            errno = 0;
            expected_value = wcstoull(expected, &expected_end, 16);
            if (errno == ERANGE || expected_end == expected ||
                expected_end == NULL || *expected_end != L'\0') {
                return 0;
            }

            *comparison = actual_value < expected_value ? -1 :
                (actual_value > expected_value ? 1 : 0);
            return 1;
        } else {
            long long actual_value;
            long long expected_value;

            errno = 0;
            actual_value = wcstoll(actual, &actual_end, 10);
            if (errno == ERANGE || actual_end == actual ||
                actual_end == NULL || *actual_end != L'\0') {
                return 0;
            }

            errno = 0;
            expected_value = wcstoll(expected, &expected_end, 10);
            if (errno == ERANGE || expected_end == expected ||
                expected_end == NULL || *expected_end != L'\0') {
                return 0;
            }

            *comparison = actual_value < expected_value ? -1 :
                (actual_value > expected_value ? 1 : 0);
            return 1;
        }
    }
    case TDH_INTYPE_UINT8:
    case TDH_INTYPE_UINT16:
    case TDH_INTYPE_UINT32:
    case TDH_INTYPE_UINT64:
    case TDH_INTYPE_FILETIME:
    case TDH_INTYPE_HEXINT32:
    case TDH_INTYPE_HEXINT64:
    case TDH_INTYPE_POINTER:
    case TDH_INTYPE_SIZET: {
        unsigned long long actual_value;
        unsigned long long expected_value;
        int base = in_type == TDH_INTYPE_HEXINT32 ||
            in_type == TDH_INTYPE_HEXINT64 ||
            in_type == TDH_INTYPE_POINTER ||
            in_type == TDH_INTYPE_SIZET ||
            (in_type != TDH_INTYPE_FILETIME && is_hex_output_type(out_type))
                ? 16
                : 10;

        if (expected[0] == L'-' || expected[0] == L'+') {
            return 0;
        }

        errno = 0;
        actual_value = wcstoull(actual, &actual_end, base);
        if (errno == ERANGE || actual_end == actual ||
            actual_end == NULL || *actual_end != L'\0') {
            return 0;
        }

        errno = 0;
        expected_value = wcstoull(expected, &expected_end, base);
        if (errno == ERANGE || expected_end == expected ||
            expected_end == NULL || *expected_end != L'\0') {
            return 0;
        }

        *comparison = actual_value < expected_value ? -1 :
            (actual_value > expected_value ? 1 : 0);
        return 1;
    }
    case TDH_INTYPE_FLOAT:
    case TDH_INTYPE_DOUBLE: {
        double actual_value;
        double expected_value;

        errno = 0;
        actual_value = wcstod(actual, &actual_end);
        if (errno == ERANGE || actual_end == actual ||
            actual_end == NULL || *actual_end != L'\0' ||
            !isfinite(actual_value)) {
            return 0;
        }

        errno = 0;
        expected_value = wcstod(expected, &expected_end);
        if (errno == ERANGE || expected_end == expected ||
            expected_end == NULL || *expected_end != L'\0' ||
            !isfinite(expected_value)) {
            return 0;
        }

        *comparison = actual_value < expected_value ? -1 :
            (actual_value > expected_value ? 1 : 0);
        return 1;
    }
    default:
        return 0;
    }
}

static int search_property_text_matches(
    USHORT in_type,
    USHORT out_type,
    const wchar_t *actual,
    const COMTRACE_ETL_FILTERS *filters
)
{
    int comparison;

    if (actual == NULL || filters == NULL) {
        return 0;
    }

    if (filters->property_match_mode == COMTRACE_PROPERTY_MATCH_CONTAINS) {
        if (in_type != TDH_INTYPE_ANSISTRING &&
            in_type != TDH_INTYPE_UNICODESTRING) {
            return 0;
        }
        return wcsstr(actual, filters->property_value) != NULL;
    }

    if (filters->property_match_mode == COMTRACE_PROPERTY_MATCH_EQUALS) {
        return wcscmp(actual, filters->property_value) == 0;
    }

    if (!compare_search_numbers(
            in_type,
            out_type,
            actual,
            filters->property_value,
            &comparison)) {
        return 0;
    }

    switch (filters->property_match_mode) {
    case COMTRACE_PROPERTY_MATCH_GREATER_THAN:
        return comparison > 0;
    case COMTRACE_PROPERTY_MATCH_GREATER_OR_EQUAL:
        return comparison >= 0;
    case COMTRACE_PROPERTY_MATCH_LESS_THAN:
        return comparison < 0;
    case COMTRACE_PROPERTY_MATCH_LESS_OR_EQUAL:
        return comparison <= 0;
    default:
        return 0;
    }
}

static int format_search_property_value(
    USHORT in_type,
    USHORT out_type,
    const BYTE *value,
    ULONG value_size,
    ULONG pointer_size,
    wchar_t *text,
    size_t text_capacity
)
{
    if (value == NULL || value_size == 0U || text == NULL ||
        text_capacity == 0U) {
        return 0;
    }

    text[0] = L'\0';

    if (out_type == TDH_OUTTYPE_IPV4) {
        if (in_type != TDH_INTYPE_UINT32 || value_size != 4U) {
            return 0;
        }
        _snwprintf_s(
            text,
            text_capacity,
            _TRUNCATE,
            L"%u.%u.%u.%u",
            (unsigned int)value[0],
            (unsigned int)value[1],
            (unsigned int)value[2],
            (unsigned int)value[3]
        );
        return 1;
    }

    if (out_type == TDH_OUTTYPE_IPV6) {
        if (in_type != TDH_INTYPE_BINARY || value_size != 16U ||
            InetNtopW(AF_INET6, value, text, text_capacity) == NULL) {
            return 0;
        }
        return 1;
    }

    if (out_type == TDH_OUTTYPE_SOCKETADDRESS) {
        return in_type == TDH_INTYPE_BINARY &&
            format_socket_address(value, value_size, text, text_capacity);
    }

    switch (in_type) {
    case TDH_INTYPE_UNICODESTRING: {
        size_t content_size;
        size_t displayed_size;
        int terminated;
        size_t character_count;

        if (!comtrace_scan_string(
                value,
                value_size,
                sizeof(wchar_t),
                (size_t)value_size,
                &content_size,
                &displayed_size,
                &terminated)) {
            return 0;
        }

        character_count = content_size / sizeof(wchar_t);
        if (character_count >= text_capacity) {
            return 0;
        }

        CopyMemory(text, value, content_size);
        text[character_count] = L'\0';
        return 1;
    }
    case TDH_INTYPE_ANSISTRING: {
        size_t content_size;
        size_t displayed_size;
        int terminated;
        int converted;

        if (!comtrace_scan_string(
                value,
                value_size,
                1U,
                (size_t)value_size,
                &content_size,
                &displayed_size,
                &terminated) ||
            content_size > 1023U) {
            return 0;
        }

        {
            UINT code_page = is_utf8_string_output_type(out_type)
                ? CP_UTF8
                : CP_ACP;
            DWORD conversion_flags = code_page == CP_UTF8
                ? MB_ERR_INVALID_CHARS
                : 0U;

            converted = MultiByteToWideChar(
                code_page,
                conversion_flags,
                (LPCCH)value,
                (int)content_size,
                text,
                (int)text_capacity - 1
            );
        }
        if (converted <= 0 || (size_t)converted >= text_capacity) {
            return 0;
        }
        text[converted] = L'\0';
        return 1;
    }
    case TDH_INTYPE_ANSICHAR:
        if (value_size != 1U) {
            return 0;
        }
        if (MultiByteToWideChar(
                CP_ACP,
                0U,
                (LPCCH)value,
                1,
                text,
                (int)text_capacity - 1
            ) != 1) {
            return 0;
        }
        text[1] = L'\0';
        return 1;
    case TDH_INTYPE_UNICODECHAR:
        if (value_size != sizeof(wchar_t)) {
            return 0;
        }
        CopyMemory(text, value, sizeof(wchar_t));
        text[1] = L'\0';
        return 1;
    case TDH_INTYPE_INT8: {
        int8_t number;
        if (value_size != sizeof(number)) return 0;
        CopyMemory(&number, value, sizeof(number));
        _snwprintf_s(text, text_capacity, _TRUNCATE, L"%d", (int)number);
        return 1;
    }
    case TDH_INTYPE_UINT8: {
        uint8_t number;
        if (value_size != sizeof(number)) return 0;
        CopyMemory(&number, value, sizeof(number));
        if (out_type == TDH_OUTTYPE_HEXINT8) {
            _snwprintf_s(text, text_capacity, _TRUNCATE, L"0x%02X", (unsigned int)number);
        } else {
            _snwprintf_s(text, text_capacity, _TRUNCATE, L"%u", (unsigned int)number);
        }
        return 1;
    }
    case TDH_INTYPE_INT16: {
        int16_t number;
        if (value_size != sizeof(number)) return 0;
        CopyMemory(&number, value, sizeof(number));
        _snwprintf_s(text, text_capacity, _TRUNCATE, L"%d", (int)number);
        return 1;
    }
    case TDH_INTYPE_UINT16: {
        uint16_t number;
        if (value_size != sizeof(number)) return 0;
        CopyMemory(&number, value, sizeof(number));
        if (out_type == TDH_OUTTYPE_HEXINT16) {
            _snwprintf_s(text, text_capacity, _TRUNCATE, L"0x%04X", (unsigned int)number);
        } else if (out_type == TDH_OUTTYPE_PORT) {
            _snwprintf_s(text, text_capacity, _TRUNCATE, L"%u", (unsigned int)ntohs(number));
        } else {
            _snwprintf_s(text, text_capacity, _TRUNCATE, L"%u", (unsigned int)number);
        }
        return 1;
    }
    case TDH_INTYPE_INT32: {
        int32_t number;
        if (value_size != sizeof(number)) return 0;
        CopyMemory(&number, value, sizeof(number));
        if (is_hex_output_type(out_type)) {
            uint32_t bits;
            CopyMemory(&bits, value, sizeof(bits));
            _snwprintf_s(text, text_capacity, _TRUNCATE, L"0x%08X", (unsigned int)bits);
        } else {
            _snwprintf_s(text, text_capacity, _TRUNCATE, L"%ld", (long)number);
        }
        return 1;
    }
    case TDH_INTYPE_UINT32: {
        uint32_t number;
        if (value_size != sizeof(number)) return 0;
        CopyMemory(&number, value, sizeof(number));
        if (is_hex_output_type(out_type)) {
            _snwprintf_s(text, text_capacity, _TRUNCATE, L"0x%08lX", (unsigned long)number);
        } else {
            _snwprintf_s(text, text_capacity, _TRUNCATE, L"%lu", (unsigned long)number);
        }
        return 1;
    }
    case TDH_INTYPE_INT64: {
        int64_t number;
        if (value_size != sizeof(number)) return 0;
        CopyMemory(&number, value, sizeof(number));
        _snwprintf_s(text, text_capacity, _TRUNCATE, L"%lld", (long long)number);
        return 1;
    }
    case TDH_INTYPE_UINT64:
    case TDH_INTYPE_FILETIME: {
        ULONGLONG number;
        if (value_size != sizeof(number)) return 0;
        CopyMemory(&number, value, sizeof(number));
        if (in_type == TDH_INTYPE_UINT64 &&
            (out_type == TDH_OUTTYPE_HEXINT64 ||
             out_type == TDH_OUTTYPE_CODE_POINTER)) {
            _snwprintf_s(text, text_capacity, _TRUNCATE, L"0x%016llX", (unsigned long long)number);
        } else {
            _snwprintf_s(text, text_capacity, _TRUNCATE, L"%llu", (unsigned long long)number);
        }
        return 1;
    }
    case TDH_INTYPE_FLOAT: {
        float number;
        if (value_size != sizeof(number)) return 0;
        CopyMemory(&number, value, sizeof(number));
        _snwprintf_s(text, text_capacity, _TRUNCATE, L"%.9g", (double)number);
        return 1;
    }
    case TDH_INTYPE_DOUBLE: {
        double number;
        if (value_size != sizeof(number)) return 0;
        CopyMemory(&number, value, sizeof(number));
        _snwprintf_s(text, text_capacity, _TRUNCATE, L"%.17g", number);
        return 1;
    }
    case TDH_INTYPE_BOOLEAN: {
        ULONGLONG number = 0ULL;
        if (value_size != 1U && value_size != 2U &&
            value_size != 4U && value_size != 8U) {
            return 0;
        }
        CopyMemory(&number, value, value_size);
        wcscpy_s(text, text_capacity, number == 0ULL ? L"false" : L"true");
        return 1;
    }
    case TDH_INTYPE_GUID: {
        GUID guid;
        if (value_size != sizeof(guid)) return 0;
        CopyMemory(&guid, value, sizeof(guid));
        _snwprintf_s(
            text,
            text_capacity,
            _TRUNCATE,
            L"{%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
            (unsigned long)guid.Data1,
            (unsigned int)guid.Data2,
            (unsigned int)guid.Data3,
            (unsigned int)guid.Data4[0],
            (unsigned int)guid.Data4[1],
            (unsigned int)guid.Data4[2],
            (unsigned int)guid.Data4[3],
            (unsigned int)guid.Data4[4],
            (unsigned int)guid.Data4[5],
            (unsigned int)guid.Data4[6],
            (unsigned int)guid.Data4[7]
        );
        return 1;
    }
    case TDH_INTYPE_HEXINT32: {
        ULONG number;
        if (value_size != sizeof(number)) return 0;
        CopyMemory(&number, value, sizeof(number));
        _snwprintf_s(text, text_capacity, _TRUNCATE, L"0x%08lX", (unsigned long)number);
        return 1;
    }
    case TDH_INTYPE_HEXINT64: {
        ULONGLONG number;
        if (value_size != sizeof(number)) return 0;
        CopyMemory(&number, value, sizeof(number));
        _snwprintf_s(text, text_capacity, _TRUNCATE, L"0x%016llX", (unsigned long long)number);
        return 1;
    }
    case TDH_INTYPE_POINTER:
    case TDH_INTYPE_SIZET: {
        ULONGLONG number = 0ULL;
        if ((pointer_size != 4U && pointer_size != 8U) ||
            value_size != pointer_size) {
            return 0;
        }
        CopyMemory(&number, value, value_size);
        _snwprintf_s(text, text_capacity, _TRUNCATE, L"0x%llX", (unsigned long long)number);
        return 1;
    }
    case TDH_INTYPE_SYSTEMTIME: {
        ComTraceSystemTime time_value;
        if (!comtrace_decode_systemtime(value, value_size, &time_value)) {
            return 0;
        }
        _snwprintf_s(
            text,
            text_capacity,
            _TRUNCATE,
            L"%04u-%02u-%02uT%02u:%02u:%02u.%03u",
            (unsigned int)time_value.year,
            (unsigned int)time_value.month,
            (unsigned int)time_value.day,
            (unsigned int)time_value.hour,
            (unsigned int)time_value.minute,
            (unsigned int)time_value.second,
            (unsigned int)time_value.milliseconds
        );
        return 1;
    }
    case TDH_INTYPE_SID: {
        PSID sid = (PSID)value;
        LPWSTR sid_text = NULL;
        size_t sid_size;
        size_t sid_text_length;
        int copied;

        if (value_size < 8U) {
            return 0;
        }

        sid_size = 8U + ((size_t)value[1] * sizeof(ULONG));
        if (sid_size > (size_t)value_size ||
            !IsValidSid(sid) ||
            (size_t)GetLengthSid(sid) != sid_size ||
            !ConvertSidToStringSidW(sid, &sid_text)) {
            return 0;
        }

        sid_text_length = wcslen(sid_text);
        if (sid_text_length >= text_capacity) {
            LocalFree(sid_text);
            return 0;
        }

        copied = wcscpy_s(text, text_capacity, sid_text);
        LocalFree(sid_text);
        return copied == 0;
    }
    default:
        return 0;
    }
}

static int hex_character_value(wchar_t character)
{
    if (character >= L'0' && character <= L'9') {
        return (int)(character - L'0');
    }
    if (character >= L'a' && character <= L'f') {
        return (int)(character - L'a') + 10;
    }
    if (character >= L'A' && character <= L'F') {
        return (int)(character - L'A') + 10;
    }
    return -1;
}

static int read_hex_byte(
    const wchar_t *text,
    BYTE *value
)
{
    int high;
    int low;

    if (text == NULL || value == NULL) {
        return 0;
    }

    high = hex_character_value(text[0]);
    low = hex_character_value(text[1]);
    if (high < 0 || low < 0) {
        return 0;
    }

    *value = (BYTE)((high << 4) | low);
    return 1;
}

static int get_hex_pattern_size(
    const wchar_t *expected,
    size_t *pattern_size
)
{
    size_t expected_length;

    if (expected == NULL || pattern_size == NULL ||
        _wcsnicmp(expected, L"hex:", 4U) != 0) {
        return 0;
    }

    expected_length = wcslen(expected);
    if (expected_length < 4U ||
        ((expected_length - 4U) % 2U) != 0U) {
        return 0;
    }

    *pattern_size = (expected_length - 4U) / 2U;
    return 1;
}

static int search_binary_property_matches(
    const BYTE *value,
    ULONG value_size,
    const wchar_t *expected
)
{
    size_t pattern_size;
    size_t index;

    if (value == NULL ||
        !get_hex_pattern_size(expected, &pattern_size) ||
        pattern_size != (size_t)value_size) {
        return 0;
    }

    for (index = 0U; index < pattern_size; ++index) {
        BYTE expected_byte;

        if (!read_hex_byte(expected + 4U + index * 2U, &expected_byte) ||
            value[index] != expected_byte) {
            return 0;
        }
    }

    return 1;
}

static int search_binary_property_contains(
    const BYTE *value,
    ULONG value_size,
    const wchar_t *expected
)
{
    size_t pattern_size;
    size_t offset;

    if (value == NULL ||
        !get_hex_pattern_size(expected, &pattern_size) ||
        pattern_size == 0U ||
        pattern_size > (size_t)value_size) {
        return 0;
    }

    for (offset = 0U; offset <= (size_t)value_size - pattern_size; ++offset) {
        size_t index;
        int matches = 1;

        for (index = 0U; index < pattern_size; ++index) {
            BYTE expected_byte;

            if (!read_hex_byte(
                    expected + 4U + index * 2U,
                    &expected_byte) ||
                value[offset + index] != expected_byte) {
                matches = 0;
                break;
            }
        }

        if (matches) {
            return 1;
        }
    }

    return 0;
}

static int get_event_property_name(
    const BYTE *metadata_buffer,
    ULONG metadata_size,
    const EVENT_PROPERTY_INFO *property,
    const wchar_t **name
)
{
    ULONG name_offset;
    ULONG remaining;
    ULONG name_length = 0U;

    if (metadata_buffer == NULL || property == NULL || name == NULL) {
        return 0;
    }

    name_offset = property->NameOffset;
    if (name_offset >= metadata_size ||
        (name_offset % (ULONG)sizeof(wchar_t)) != 0U) {
        return 0;
    }

    *name = (const wchar_t *)(metadata_buffer + name_offset);
    remaining = (metadata_size - name_offset) / (ULONG)sizeof(wchar_t);
    while (name_length < remaining && (*name)[name_length] != L'\0') {
        ++name_length;
    }

    return name_length < remaining;
}

static int event_property_value_matches(
    const EVENT_RECORD *event_record,
    const EVENT_PROPERTY_INFO *property,
    const wchar_t *name,
    const wchar_t *const *parent_names,
    const ULONG *parent_indices,
    ULONG parent_count,
    const COMTRACE_ETL_FILTERS *filters,
    ULONG pointer_size
)
{
    COMTRACE_ETL_PROPERTY_VALUE property_value;
    COMTRACE_ETL_PROPERTY_READ_STATUS read_status;
    USHORT in_type;
    USHORT out_type;
    BYTE *value;
    ULONG value_size;
    wchar_t actual[1024];
    int matched = 0;

    if (property == NULL ||
        name == NULL ||
        filters == NULL ||
        (property->Flags & PropertyStruct) != 0U) {
        return 0;
    }

    if (event_property_is_array(property)) {
        if (!filters->has_property_index) {
            return 0;
        }
    } else if (filters->has_property_index) {
        return 0;
    }

    if ((property->Flags & PropertyHasCustomSchema) != 0U) {
        in_type = property->customSchemaType.InType;
        out_type = property->customSchemaType.OutType;
    } else {
        in_type = property->nonStructType.InType;
        out_type = property->nonStructType.OutType;
    }
    ZeroMemory(&property_value, sizeof(property_value));
    read_status = comtrace_etl_read_property_value(
        event_record,
        property,
        name,
        parent_names,
        parent_indices,
        parent_count,
        filters->has_property_index
            ? filters->property_index
            : 0xFFFFFFFFUL,
        &property_value
    );
    if (read_status != COMTRACE_ETL_PROPERTY_READ_SUCCESS) {
        comtrace_etl_property_value_destroy(&property_value);
        return 0;
    }

    value = property_value.data;
    value_size = property_value.size;
    if ((property->Flags & PropertyHasCustomSchema) != 0U) {
        if (filters->property_match_mode == COMTRACE_PROPERTY_MATCH_EQUALS) {
            matched = search_binary_property_matches(
                value,
                value_size,
                filters->property_value
            );
        } else if (filters->property_match_mode == COMTRACE_PROPERTY_MATCH_CONTAINS) {
            matched = search_binary_property_contains(
                value,
                value_size,
                filters->property_value
            );
        }
        comtrace_etl_property_value_destroy(&property_value);
        return matched;
    }

    if (comtrace_etl_normalize_property_payload(
            &in_type,
            &value,
            &value_size,
            pointer_size)) {
        if (in_type == TDH_INTYPE_BINARY &&
            out_type != TDH_OUTTYPE_IPV6 &&
            out_type != TDH_OUTTYPE_SOCKETADDRESS &&
            filters->property_match_mode == COMTRACE_PROPERTY_MATCH_EQUALS) {
            matched = search_binary_property_matches(
                value,
                value_size,
                filters->property_value
            );
        } else if (in_type == TDH_INTYPE_BINARY &&
                   out_type != TDH_OUTTYPE_IPV6 &&
                   out_type != TDH_OUTTYPE_SOCKETADDRESS &&
                   filters->property_match_mode == COMTRACE_PROPERTY_MATCH_CONTAINS) {
            matched = search_binary_property_contains(
                value,
                value_size,
                filters->property_value
            );
        } else if (format_search_property_value(
                in_type,
                out_type,
                value,
                value_size,
                pointer_size,
                actual,
                _countof(actual)
            )) {
            matched = search_property_text_matches(in_type, out_type, actual, filters);
        }
    }

    comtrace_etl_property_value_destroy(&property_value);
    return matched;
}

static int event_struct_property_matches(
    const EVENT_RECORD *event_record,
    const BYTE *metadata_buffer,
    ULONG metadata_size,
    const EVENT_PROPERTY_INFO *properties,
    ULONG property_count,
    const EVENT_PROPERTY_INFO *property,
    const wchar_t *name,
    const wchar_t *path,
    const wchar_t *const *parent_names,
    const ULONG *parent_indices,
    ULONG parent_count,
    const COMTRACE_ETL_FILTERS *filters,
    ULONG pointer_size
)
{
    ULONG member_start;
    ULONG member_count;
    ULONG member_index;
    size_t component_length;
    const wchar_t *separator;
    const wchar_t *nested_names[COMTRACE_ETL_MAX_PROPERTY_DEPTH];
    ULONG nested_indices[COMTRACE_ETL_MAX_PROPERTY_DEPTH];
    ULONG index;

    if (property == NULL || name == NULL || path == NULL ||
        path[0] == L'\0' ||
        (property->Flags & PropertyStruct) == 0U ||
        parent_count >= COMTRACE_ETL_MAX_PROPERTY_DEPTH) {
        return 0;
    }

    if (event_property_is_array(property)) {
        if (!filters->property_index_on_struct ||
            parent_count != filters->property_index_component) {
            return 0;
        }
    } else if (filters->property_index_on_struct &&
               parent_count == filters->property_index_component) {
        return 0;
    }

    member_start = property->structType.StructStartIndex;
    member_count = property->structType.NumOfStructMembers;
    if (member_start > property_count ||
        member_count > property_count - member_start) {
        return 0;
    }

    for (index = 0U; index < parent_count; ++index) {
        nested_names[index] = parent_names[index];
        nested_indices[index] = parent_indices[index];
    }
    nested_names[parent_count] = name;
    nested_indices[parent_count] =
        filters->property_index_on_struct &&
        parent_count == filters->property_index_component
            ? filters->property_index
            : 0xFFFFFFFFUL;

    separator = wcschr(path, L'.');
    component_length = separator == NULL
        ? wcslen(path)
        : (size_t)(separator - path);

    for (member_index = member_start;
         member_index < member_start + member_count;
         ++member_index) {
        const EVENT_PROPERTY_INFO *member = &properties[member_index];
        const wchar_t *member_name;
        size_t member_name_length;

        if (!get_event_property_name(
                metadata_buffer,
                metadata_size,
                member,
                &member_name)) {
            continue;
        }

        member_name_length = wcslen(member_name);
        if (member_name_length != component_length ||
            wmemcmp(member_name, path, component_length) != 0) {
            continue;
        }

        if (separator != NULL) {
            return event_struct_property_matches(
                event_record,
                metadata_buffer,
                metadata_size,
                properties,
                property_count,
                member,
                member_name,
                separator + 1,
                nested_names,
                nested_indices,
                parent_count + 1U,
                filters,
                pointer_size
            );
        }

        {
            COMTRACE_ETL_FILTERS leaf_filters = *filters;

            if (leaf_filters.property_index_on_struct) {
                leaf_filters.has_property_index = FALSE;
                leaf_filters.property_index_on_struct = FALSE;
            }

            return event_property_value_matches(
                event_record,
                member,
                member_name,
                nested_names,
                nested_indices,
                parent_count + 1U,
                &leaf_filters,
                pointer_size
            );
        }
    }

    return 0;
}

static int event_property_matches_filter(
    const EVENT_RECORD *event_record,
    const COMTRACE_ETL_FILTERS *filters,
    ULONG pointer_size
)
{
    COMTRACE_ETL_EVENT_INFO decoded_info;
    COMTRACE_ETL_EVENT_INFO_STATUS info_status;
    TRACE_EVENT_INFO *event_info;
    const BYTE *buffer;
    ULONG buffer_size;
    ULONG index;
    const wchar_t *const no_parent_names[1] = { NULL };
    const ULONG no_parent_indices[1] = { 0xFFFFFFFFUL };
    const wchar_t *separator;
    size_t root_name_length;
    int matched = 0;

    ZeroMemory(&decoded_info, sizeof(decoded_info));
    info_status = comtrace_etl_event_info_open(event_record, &decoded_info);
    if (info_status != COMTRACE_ETL_EVENT_INFO_SUCCESS) {
        return 0;
    }

    event_info = decoded_info.info;
    buffer = decoded_info.buffer;
    buffer_size = decoded_info.size;
    separator = wcschr(filters->property_name, L'.');
    root_name_length = separator == NULL
        ? wcslen(filters->property_name)
        : (size_t)(separator - filters->property_name);

    for (index = 0U; index < event_info->TopLevelPropertyCount; ++index) {
        const EVENT_PROPERTY_INFO *property =
            &event_info->EventPropertyInfoArray[index];
        const wchar_t *name;

        if (!get_event_property_name(
                buffer,
                buffer_size,
                property,
                &name)) {
            break;
        }

        if (wcslen(name) != root_name_length ||
            wmemcmp(name, filters->property_name, root_name_length) != 0) {
            continue;
        }

        if (separator == NULL) {
            if (!filters->property_index_on_struct) {
                matched = event_property_value_matches(
                    event_record,
                    property,
                    name,
                    no_parent_names,
                    no_parent_indices,
                    0U,
                    filters,
                    pointer_size
                );
            }
        } else if (separator[1] != L'\\0') {
            matched = event_struct_property_matches(
                event_record,
                buffer,
                buffer_size,
                event_info->EventPropertyInfoArray,
                event_info->PropertyCount,
                property,
                name,
                separator + 1,
                no_parent_names,
                no_parent_indices,
                0U,
                filters,
                pointer_size
            );
        }

        break;
    }

    comtrace_etl_event_info_close(&decoded_info);
    return matched;
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
        ) &&
        !is_com_lifetime_event_record(event_record)) {
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

    operation = NULL;
    phase = NULL;
    if (context->filters.has_class_id ||
        context->filters.has_interface_id ||
        context->filters.has_object_id) {
        COMTRACE_COM_OBJECT_IDENTITY identity;

        if (comtrace_extract_com_object_identity(event_record, &identity) !=
            COMTRACE_IDENTITY_SUCCESS) {
            return 0;
        }
        if (context->filters.has_class_id &&
            (!identity.has_clsid ||
             memcmp(&identity.clsid, &context->filters.class_id, sizeof(GUID)) != 0)) {
            return 0;
        }
        if (context->filters.has_interface_id &&
            (!identity.has_iid ||
             memcmp(&identity.iid, &context->filters.interface_id, sizeof(GUID)) != 0)) {
            return 0;
        }
        if (context->filters.has_object_id &&
            (!identity.has_oid || identity.oid != context->filters.object_id)) {
            return 0;
        }
    }

    if (context->filters.has_lifetime_object_id) {
        ULONGLONG object_instance_id = 0ULL;
        if (comtrace_extract_uint64_property(
                event_record, L"ObjectInstanceId", &object_instance_id
            ) != COMTRACE_IDENTITY_SUCCESS ||
            object_instance_id != context->filters.lifetime_object_id) {
            return 0;
        }
    }

    if (context->filters.has_activity_id) {
        COMTRACE_ACTIVITY_IDS activity_ids;
        if (comtrace_extract_activity_ids(event_record, &activity_ids) !=
            COMTRACE_IDENTITY_SUCCESS ||
            ((!activity_ids.has_activity_id ||
              memcmp(&activity_ids.activity_id, &context->filters.activity_id, sizeof(GUID)) != 0) &&
             (!activity_ids.has_related_activity_id ||
              memcmp(&activity_ids.related_activity_id, &context->filters.activity_id, sizeof(GUID)) != 0))) {
            return 0;
        }
    }

    if (context->filters.has_property_name &&
        !event_property_matches_filter(
            event_record,
            &context->filters,
            context->pointer_size
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
    add_process_lifecycle_event(context, event_record);

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
    add_rpc_timeline_event(context, event_record);
    add_dde_timeline_event(context, event_record);
    add_relationship_graph_event(context, event_record);
    add_com_lifetime_analysis_event(context, event_record);

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
        L"sample=%lu time_ticks=",
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
    print_com_rundown_event_classification(owned_event);
    print_com_runtime_event_classification(owned_event);
    print_rpc_event_classification(owned_event);
    print_rpcss_event_classification(owned_event);
    print_com_lifetime_event_classification(owned_event);
    print_ole_event_classification(owned_event);
    print_event_properties(owned_event, context->pointer_size);

    comtrace_event_record_destroy(owned_record);
}
static void destroy_inspect_context(ETL_INSPECT_CONTEXT *context)
{
    if (context == NULL) {
        return;
    }

    comtrace_process_lifecycle_catalog_destroy(&context->process_lifecycle);
    comtrace_relationship_graph_destroy(&context->relationship_graph);
    comtrace_dde_timeline_destroy(&context->dde_timeline);
    comtrace_rpc_timeline_destroy(&context->rpc_timeline);
    comtrace_ole_timeline_destroy(&context->ole_timeline);
    comtrace_timeline_destroy(&context->timeline);
    comtrace_lifetime_analysis_destroy(&context->lifetime_analysis);
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

    ZeroMemory(&context, sizeof(context));

    if (etl_path == NULL || etl_path[0] == L'\0') {
        fwprintf(stderr, L"An ETL input path is required.\n");
        destroy_inspect_context(&context);
        return EXIT_FAILURE;
    }

    ZeroMemory(&logfile, sizeof(logfile));
    comtrace_event_catalog_init(&context.event_catalog);
    comtrace_timeline_init(&context.timeline);
    context.timeline_status = COMTRACE_TIMELINE_SUCCESS;
    comtrace_ole_timeline_init(&context.ole_timeline);
    context.ole_timeline_status = COMTRACE_OLE_TIMELINE_SUCCESS;
    comtrace_rpc_timeline_init(&context.rpc_timeline);
    context.rpc_timeline_status = COMTRACE_RPC_TIMELINE_SUCCESS;
    comtrace_dde_timeline_init(&context.dde_timeline);
    context.dde_timeline_status = COMTRACE_DDE_TIMELINE_SUCCESS;
    comtrace_relationship_graph_init(&context.relationship_graph);
    context.graph_status = COMTRACE_GRAPH_SUCCESS;
    comtrace_process_lifecycle_catalog_init(&context.process_lifecycle);
    comtrace_lifetime_analysis_init(&context.lifetime_analysis);
    context.lifetime_status = COMTRACE_LIFETIME_SUCCESS;

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
    logfile.ProcessTraceMode =
        PROCESS_TRACE_MODE_EVENT_RECORD |
        PROCESS_TRACE_MODE_RAW_TIMESTAMP;
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

    add_lifetime_records_to_graph(&context);
    if (comtrace_dde_timeline_correlate(&context.dde_timeline) !=
        COMTRACE_DDE_TIMELINE_SUCCESS) {
        context.dde_timeline_status = COMTRACE_DDE_TIMELINE_ALLOCATION_FAILED;
    }
    print_com_timeline_summary(&context);
    print_ole_timeline_summary(&context);
    print_rpc_timeline_summary(&context);
    print_dde_timeline_summary(&context);
    print_com_lifetime_summary(&context);
    print_relationship_graph_summary(&context.relationship_graph);
    print_process_lifecycle_summary(
        &context.process_lifecycle,
        context.process_lifecycle_failures
    );

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

    if (context.dde_timeline_status != COMTRACE_DDE_TIMELINE_SUCCESS) {
        fwprintf(stderr, L"DDE timeline processing failed.\n");
        result = EXIT_FAILURE;
    }

    if (context.lifetime_status != COMTRACE_LIFETIME_SUCCESS) {
        fwprintf(stderr, L"COM lifetime analysis failed with status %u.\n",
            (unsigned int)context.lifetime_status);
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
