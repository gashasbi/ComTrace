#include "com_identity.h"
#include "etl_decoder.h"
#include "etl_property_format.h"

#include <limits.h>
#include <stdlib.h>
#include <wchar.h>

static COMTRACE_IDENTITY_STATUS extract_property_value(
    const EVENT_RECORD *event_record,
    const wchar_t *property_name,
    USHORT expected_type,
    BYTE *value,
    ULONG expected_size
)
{
    COMTRACE_ETL_EVENT_INFO event_info;
    COMTRACE_ETL_EVENT_INFO_STATUS info_status;
    ULONG index;
    COMTRACE_IDENTITY_STATUS result = COMTRACE_IDENTITY_PROPERTY_NOT_FOUND;

    if (event_record == NULL ||
        property_name == NULL ||
        property_name[0] == L'\\0' ||
        value == NULL ||
        expected_size == 0U ||
        expected_size > sizeof(GUID)) {
        return COMTRACE_IDENTITY_INVALID_ARGUMENT;
    }

    ZeroMemory(&event_info, sizeof(event_info));
    info_status = comtrace_etl_event_info_open(event_record, &event_info);
    if (info_status != COMTRACE_ETL_EVENT_INFO_SUCCESS) {
        return COMTRACE_IDENTITY_METADATA_UNAVAILABLE;
    }

    for (index = 0U; index < event_info.top_level_property_count; ++index) {
        const EVENT_PROPERTY_INFO *property =
            &event_info.info->EventPropertyInfoArray[index];
        const wchar_t *name =
            (const wchar_t *)(event_info.buffer + property->NameOffset);
        USHORT input_type =
            (property->Flags & PropertyHasCustomSchema) != 0U
                ? property->customSchemaType.InType
                : property->nonStructType.InType;
        COMTRACE_ETL_PROPERTY_VALUE property_value;
        COMTRACE_ETL_PROPERTY_READ_STATUS read_status;

        if (wcscmp(name, property_name) != 0) {
            continue;
        }

        if ((property->Flags & (
                PropertyStruct |
                PropertyParamCount |
                PropertyParamFixedCount |
                PropertyWBEMXmlFragment |
                PropertyHasCustomSchema
            )) != 0U ||
            input_type != expected_type) {
            result = COMTRACE_IDENTITY_PROPERTY_UNSUPPORTED;
            break;
        }

        ZeroMemory(&property_value, sizeof(property_value));
        read_status = comtrace_etl_read_property_value(
            event_record,
            property,
            name,
            NULL,
            NULL,
            0U,
            0xFFFFFFFFUL,
            &property_value
        );
        if (read_status != COMTRACE_ETL_PROPERTY_READ_SUCCESS) {
            result = COMTRACE_IDENTITY_VALUE_UNAVAILABLE;
            break;
        }

        if (property_value.size != expected_size) {
            comtrace_etl_property_value_destroy(&property_value);
            result = COMTRACE_IDENTITY_VALUE_UNAVAILABLE;
            break;
        }

        CopyMemory(value, property_value.data, expected_size);
        comtrace_etl_property_value_destroy(&property_value);
        result = COMTRACE_IDENTITY_SUCCESS;
        break;
    }

    comtrace_etl_event_info_close(&event_info);
    return result;
}

COMTRACE_IDENTITY_STATUS comtrace_extract_guid_property(
    const EVENT_RECORD *event_record,
    const wchar_t *property_name,
    GUID *value
)
{
    GUID candidate;
    COMTRACE_IDENTITY_STATUS status;

    if (value == NULL) {
        return COMTRACE_IDENTITY_INVALID_ARGUMENT;
    }

    status = extract_property_value(
        event_record,
        property_name,
        TDH_INTYPE_GUID,
        (BYTE *)&candidate,
        (ULONG)sizeof(candidate)
    );

    if (status == COMTRACE_IDENTITY_SUCCESS) {
        CopyMemory(value, &candidate, sizeof(candidate));
    }

    return status;
}

COMTRACE_IDENTITY_STATUS comtrace_extract_uint32_property(
    const EVENT_RECORD *event_record,
    const wchar_t *property_name,
    ULONG *value
)
{
    ULONG candidate;
    COMTRACE_IDENTITY_STATUS status;

    if (value == NULL) {
        return COMTRACE_IDENTITY_INVALID_ARGUMENT;
    }

    status = extract_property_value(
        event_record,
        property_name,
        TDH_INTYPE_UINT32,
        (BYTE *)&candidate,
        (ULONG)sizeof(candidate)
    );

    if (status == COMTRACE_IDENTITY_PROPERTY_UNSUPPORTED) {
        status = extract_property_value(
            event_record,
            property_name,
            TDH_INTYPE_HEXINT32,
            (BYTE *)&candidate,
            (ULONG)sizeof(candidate)
        );
    }

    if (status == COMTRACE_IDENTITY_PROPERTY_UNSUPPORTED) {
        LONG signed_candidate = 0;

        status = extract_property_value(
            event_record,
            property_name,
            TDH_INTYPE_INT32,
            (BYTE *)&signed_candidate,
            (ULONG)sizeof(signed_candidate)
        );
        if (status == COMTRACE_IDENTITY_SUCCESS) {
            if (signed_candidate < 0) {
                status = COMTRACE_IDENTITY_PROPERTY_UNSUPPORTED;
            } else {
                candidate = (ULONG)signed_candidate;
            }
        }
    }

    if (status == COMTRACE_IDENTITY_SUCCESS) {
        *value = candidate;
    }

    return status;
}

COMTRACE_IDENTITY_STATUS comtrace_extract_int32_property(
    const EVENT_RECORD *event_record,
    const wchar_t *property_name,
    LONG *value
)
{
    LONG candidate;
    COMTRACE_IDENTITY_STATUS status;

    if (value == NULL) {
        return COMTRACE_IDENTITY_INVALID_ARGUMENT;
    }

    status = extract_property_value(
        event_record,
        property_name,
        TDH_INTYPE_INT32,
        (BYTE *)&candidate,
        (ULONG)sizeof(candidate)
    );

    if (status == COMTRACE_IDENTITY_SUCCESS) {
        *value = candidate;
    }

    return status;
}

COMTRACE_IDENTITY_STATUS comtrace_extract_uint64_property(
    const EVENT_RECORD *event_record,
    const wchar_t *property_name,
    ULONGLONG *value
)
{
    ULONGLONG candidate;
    COMTRACE_IDENTITY_STATUS status;

    if (value == NULL) {
        return COMTRACE_IDENTITY_INVALID_ARGUMENT;
    }

    status = extract_property_value(
        event_record,
        property_name,
        TDH_INTYPE_UINT64,
        (BYTE *)&candidate,
        (ULONG)sizeof(candidate)
    );

    if (status == COMTRACE_IDENTITY_PROPERTY_UNSUPPORTED) {
        status = extract_property_value(
            event_record,
            property_name,
            TDH_INTYPE_HEXINT64,
            (BYTE *)&candidate,
            (ULONG)sizeof(candidate)
        );
    }

    if (status == COMTRACE_IDENTITY_PROPERTY_UNSUPPORTED) {
        status = extract_property_value(
            event_record,
            property_name,
            TDH_INTYPE_HEXINT32,
            (BYTE *)&candidate,
            (ULONG)sizeof(candidate)
        );
    }

    if (status == COMTRACE_IDENTITY_PROPERTY_UNSUPPORTED) {
        status = extract_property_value(
            event_record,
            property_name,
            TDH_INTYPE_UINT32,
            (BYTE *)&candidate,
            (ULONG)sizeof(candidate)
        );
    }

    if (status == COMTRACE_IDENTITY_SUCCESS) {
        *value = candidate;
    }

    return status;
}

static int is_string_input_type(USHORT input_type)
{
    switch (input_type) {
    case TDH_INTYPE_UNICODESTRING:
    case TDH_INTYPE_ANSISTRING:
    case TDH_INTYPE_NONNULLTERMINATEDSTRING:
    case TDH_INTYPE_NONNULLTERMINATEDANSISTRING:
    case TDH_INTYPE_MANIFEST_COUNTEDSTRING:
    case TDH_INTYPE_COUNTEDSTRING:
    case TDH_INTYPE_REVERSEDCOUNTEDSTRING:
    case TDH_INTYPE_MANIFEST_COUNTEDANSISTRING:
    case TDH_INTYPE_COUNTEDANSISTRING:
    case TDH_INTYPE_REVERSEDCOUNTEDANSISTRING:
        return 1;
    default:
        return 0;
    }
}

COMTRACE_IDENTITY_STATUS comtrace_extract_string_property(
    const EVENT_RECORD *event_record,
    const wchar_t *property_name,
    wchar_t *value,
    size_t value_capacity
)
{
    COMTRACE_ETL_EVENT_INFO event_info;
    COMTRACE_ETL_EVENT_INFO_STATUS info_status;
    ULONG index;
    COMTRACE_IDENTITY_STATUS result = COMTRACE_IDENTITY_PROPERTY_NOT_FOUND;

    if (event_record == NULL ||
        property_name == NULL ||
        property_name[0] == L'\0' ||
        value == NULL ||
        value_capacity == 0U ||
        value_capacity > (size_t)INT_MAX) {
        return COMTRACE_IDENTITY_INVALID_ARGUMENT;
    }

    value[0] = L'\0';
    ZeroMemory(&event_info, sizeof(event_info));
    info_status = comtrace_etl_event_info_open(event_record, &event_info);
    if (info_status != COMTRACE_ETL_EVENT_INFO_SUCCESS) {
        return COMTRACE_IDENTITY_METADATA_UNAVAILABLE;
    }

    for (index = 0U; index < event_info.top_level_property_count; ++index) {
        const EVENT_PROPERTY_INFO *property =
            &event_info.info->EventPropertyInfoArray[index];
        const wchar_t *name =
            (const wchar_t *)(event_info.buffer + property->NameOffset);
        COMTRACE_ETL_PROPERTY_VALUE property_value;
        COMTRACE_ETL_PROPERTY_READ_STATUS read_status;
        USHORT input_type;
        USHORT output_type;
        USHORT normalized_type;
        BYTE *payload;
        ULONG payload_size;
        ULONG byte_count;
        ULONG text_length = 0U;

        if (wcscmp(name, property_name) != 0) {
            continue;
        }

        if ((property->Flags & (
                PropertyStruct |
                PropertyParamCount |
                PropertyParamFixedCount |
                PropertyHasCustomSchema
            )) != 0U) {
            result = COMTRACE_IDENTITY_PROPERTY_UNSUPPORTED;
            break;
        }

        input_type = property->nonStructType.InType;
        output_type = property->nonStructType.OutType;
        if (!is_string_input_type(input_type)) {
            result = COMTRACE_IDENTITY_PROPERTY_UNSUPPORTED;
            break;
        }

        ZeroMemory(&property_value, sizeof(property_value));
        read_status = comtrace_etl_read_property_value(
            event_record,
            property,
            name,
            NULL,
            NULL,
            0U,
            0xFFFFFFFFUL,
            &property_value
        );
        if (read_status != COMTRACE_ETL_PROPERTY_READ_SUCCESS) {
            result = COMTRACE_IDENTITY_VALUE_UNAVAILABLE;
            break;
        }

        payload = property_value.data;
        payload_size = property_value.size;
        normalized_type = input_type;
        if (!comtrace_etl_normalize_property_payload(
                &normalized_type,
                &payload,
                &payload_size,
                (ULONG)sizeof(void *))) {
            comtrace_etl_property_value_destroy(&property_value);
            result = COMTRACE_IDENTITY_VALUE_UNAVAILABLE;
            break;
        }

        byte_count = payload_size;
        if (normalized_type == TDH_INTYPE_UNICODESTRING) {
            const wchar_t *source;
            ULONG character_count;

            if ((byte_count % (ULONG)sizeof(wchar_t)) != 0U) {
                result = COMTRACE_IDENTITY_VALUE_UNAVAILABLE;
            } else {
                source = (const wchar_t *)payload;
                character_count = byte_count / (ULONG)sizeof(wchar_t);
                while (character_count != 0U &&
                    source[character_count - 1U] == L'\0') {
                    --character_count;
                }
                while (text_length < character_count &&
                    source[text_length] != L'\0') {
                    ++text_length;
                }
                if ((size_t)text_length >= value_capacity) {
                    result = COMTRACE_IDENTITY_VALUE_UNAVAILABLE;
                } else {
                    CopyMemory(value, source, (size_t)text_length * sizeof(wchar_t));
                    value[text_length] = L'\0';
                    result = COMTRACE_IDENTITY_SUCCESS;
                }
            }
        } else if (normalized_type == TDH_INTYPE_ANSISTRING) {
            UINT code_page = output_type == TDH_OUTTYPE_UTF8 ||
                output_type == TDH_OUTTYPE_JSON ||
                output_type == TDH_OUTTYPE_XML
                    ? CP_UTF8
                    : CP_ACP;
            DWORD conversion_flags = code_page == CP_UTF8
                ? MB_ERR_INVALID_CHARS
                : 0U;
            int converted;

            while (byte_count != 0U && payload[byte_count - 1U] == 0U) {
                --byte_count;
            }
            if (byte_count > (ULONG)INT_MAX) {
                result = COMTRACE_IDENTITY_VALUE_UNAVAILABLE;
            } else {
                converted = MultiByteToWideChar(
                    code_page,
                    conversion_flags,
                    (LPCCH)payload,
                    (int)byte_count,
                    value,
                    (int)(value_capacity - 1U)
                );
                if (converted <= 0 && byte_count != 0U) {
                    result = COMTRACE_IDENTITY_VALUE_UNAVAILABLE;
                } else {
                    value[converted] = L'\0';
                    result = COMTRACE_IDENTITY_SUCCESS;
                }
            }
        } else {
            result = COMTRACE_IDENTITY_PROPERTY_UNSUPPORTED;
        }

        comtrace_etl_property_value_destroy(&property_value);
        break;
    }

    comtrace_etl_event_info_close(&event_info);
    return result;
}

COMTRACE_IDENTITY_STATUS comtrace_extract_call_trace_id(
    const EVENT_RECORD *event_record,
    ULONGLONG *value
)
{
    ULONGLONG value64;
    ULONG value32;
    COMTRACE_IDENTITY_STATUS status;

    if (value == NULL) {
        return COMTRACE_IDENTITY_INVALID_ARGUMENT;
    }

    status = comtrace_extract_uint64_property(
        event_record,
        L"CallTraceId",
        &value64
    );
    if (status == COMTRACE_IDENTITY_SUCCESS) {
        *value = value64;
        return status;
    }

    if (status != COMTRACE_IDENTITY_PROPERTY_UNSUPPORTED) {
        return status;
    }

    status = comtrace_extract_uint32_property(
        event_record,
        L"CallTraceId",
        &value32
    );
    if (status == COMTRACE_IDENTITY_SUCCESS) {
        *value = (ULONGLONG)value32;
    }

    return status;
}

COMTRACE_IDENTITY_STATUS comtrace_extract_activity_ids(
    const EVENT_RECORD *event_record,
    COMTRACE_ACTIVITY_IDS *activity_ids
)
{
    static const GUID empty_guid = { 0 };
    COMTRACE_ACTIVITY_IDS result;
    USHORT index;

    if (event_record == NULL || activity_ids == NULL) {
        return COMTRACE_IDENTITY_INVALID_ARGUMENT;
    }

    ZeroMemory(&result, sizeof(result));

    if (memcmp(
            &event_record->EventHeader.ActivityId,
            &empty_guid,
            sizeof(empty_guid)
        ) != 0) {
        result.activity_id = event_record->EventHeader.ActivityId;
        result.has_activity_id = TRUE;
    }

    if (event_record->ExtendedDataCount != 0U &&
        event_record->ExtendedData == NULL) {
        return COMTRACE_IDENTITY_VALUE_UNAVAILABLE;
    }

    for (index = 0U; index < event_record->ExtendedDataCount; ++index) {
        const EVENT_HEADER_EXTENDED_DATA_ITEM *item =
            &event_record->ExtendedData[index];
        const EVENT_EXTENDED_ITEM_RELATED_ACTIVITYID *related;

        if (item->ExtType != EVENT_HEADER_EXT_TYPE_RELATED_ACTIVITYID) {
            continue;
        }

        if (item->DataSize < sizeof(EVENT_EXTENDED_ITEM_RELATED_ACTIVITYID) ||
            item->DataPtr == 0ULL) {
            return COMTRACE_IDENTITY_VALUE_UNAVAILABLE;
        }

        related = (const EVENT_EXTENDED_ITEM_RELATED_ACTIVITYID *)(ULONG_PTR)item->DataPtr;
        result.related_activity_id = related->RelatedActivityId;
        result.has_related_activity_id = TRUE;
        break;
    }

    CopyMemory(activity_ids, &result, sizeof(result));
    return COMTRACE_IDENTITY_SUCCESS;
}

BOOL comtrace_get_process_image_path(
    DWORD process_id,
    wchar_t *path,
    DWORD path_capacity
)
{
    HANDLE process;
    DWORD path_length;
    BOOL result;

    if (path == NULL || path_capacity == 0U) {
        return FALSE;
    }

    path[0] = L'\0';
    if (process_id == 0U) {
        return FALSE;
    }

    process = OpenProcess(
        PROCESS_QUERY_LIMITED_INFORMATION,
        FALSE,
        process_id
    );
    if (process == NULL) {
        return FALSE;
    }

    path_length = path_capacity;
    result = QueryFullProcessImageNameW(
        process,
        0U,
        path,
        &path_length
    );

    CloseHandle(process);

    if (!result || path_length == 0U || path_length >= path_capacity) {
        path[0] = L'\0';
        return FALSE;
    }

    path[path_length] = L'\0';
    return TRUE;
}

COMTRACE_IDENTITY_STATUS comtrace_extract_com_object_identity(
    const EVENT_RECORD *event_record,
    COMTRACE_COM_OBJECT_IDENTITY *identity
)
{
    COMTRACE_COM_OBJECT_IDENTITY result;
    COMTRACE_IDENTITY_STATUS status;

    if (event_record == NULL || identity == NULL) {
        return COMTRACE_IDENTITY_INVALID_ARGUMENT;
    }

    ZeroMemory(&result, sizeof(result));

    result.process_id = event_record->EventHeader.ProcessId;
    result.thread_id = event_record->EventHeader.ThreadId;
    result.has_process_id = result.process_id != 0U;
    result.has_thread_id = result.thread_id != 0U;
    result.timestamp = event_record->EventHeader.TimeStamp.QuadPart;
    result.has_timestamp = TRUE;

    status = comtrace_extract_guid_property(event_record, L"ClassId", &result.clsid);
    if (status == COMTRACE_IDENTITY_PROPERTY_NOT_FOUND) {
        status = comtrace_extract_guid_property(event_record, L"CLSID", &result.clsid);
    }
    result.has_clsid = status == COMTRACE_IDENTITY_SUCCESS;

    status = comtrace_extract_guid_property(
        event_record,
        L"TargetInterface",
        &result.iid
    );
    if (status == COMTRACE_IDENTITY_PROPERTY_NOT_FOUND) {
        status = comtrace_extract_guid_property(event_record, L"IID", &result.iid);
    }
    if (status == COMTRACE_IDENTITY_PROPERTY_NOT_FOUND) {
        status = comtrace_extract_guid_property(event_record, L"InterfaceId", &result.iid);
    }
    result.has_iid = status == COMTRACE_IDENTITY_SUCCESS;

    status = comtrace_extract_guid_property(event_record, L"IPID", &result.ipid);
    if (status == COMTRACE_IDENTITY_PROPERTY_NOT_FOUND) {
        status = comtrace_extract_guid_property(
            event_record,
            L"InterfacePointerId",
            &result.ipid
        );
    }
    result.has_ipid = status == COMTRACE_IDENTITY_SUCCESS;

    status = comtrace_extract_uint64_property(event_record, L"OID", &result.oid);
    if (status == COMTRACE_IDENTITY_PROPERTY_NOT_FOUND) {
        status = comtrace_extract_uint64_property(
            event_record,
            L"ObjectId",
            &result.oid
        );
    }
    result.has_oid = status == COMTRACE_IDENTITY_SUCCESS;

    status = comtrace_extract_call_trace_id(
        event_record,
        &result.call_trace_id
    );
    result.has_call_trace_id = status == COMTRACE_IDENTITY_SUCCESS;

    status = comtrace_extract_uint32_property(
        event_record,
        L"TargetProcessId",
        &result.target_process_id
    );
    result.has_target_process_id = status == COMTRACE_IDENTITY_SUCCESS;

    status = comtrace_extract_uint32_property(
        event_record,
        L"TargetThreadId",
        &result.target_thread_id
    );
    result.has_target_thread_id = status == COMTRACE_IDENTITY_SUCCESS;

    status = comtrace_extract_uint32_property(
        event_record,
        L"SourceProcessId",
        &result.source_process_id
    );
    result.has_source_process_id = status == COMTRACE_IDENTITY_SUCCESS;

    status = comtrace_extract_uint32_property(
        event_record,
        L"SourceThreadId",
        &result.source_thread_id
    );
    result.has_source_thread_id = status == COMTRACE_IDENTITY_SUCCESS;

    CopyMemory(identity, &result, sizeof(result));
    return COMTRACE_IDENTITY_SUCCESS;
}
