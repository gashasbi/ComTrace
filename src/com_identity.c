#include "com_identity.h"

#include <stddef.h>
#include <stdlib.h>
#include <wchar.h>
#include <tdh.h>

#define COMTRACE_MAX_EVENT_INFO_SIZE 65536U

static COMTRACE_IDENTITY_STATUS extract_property_value(
    const EVENT_RECORD *event_record,
    const wchar_t *property_name,
    USHORT expected_type,
    BYTE *value,
    ULONG expected_size
)
{
    ULONG buffer_size = 0U;
    ULONG allocated_size;
    ULONG status;
    ULONG available_count;
    ULONG index;
    BYTE *buffer;
    TRACE_EVENT_INFO *event_info;
    size_t property_offset = offsetof(TRACE_EVENT_INFO, EventPropertyInfoArray);
    COMTRACE_IDENTITY_STATUS result = COMTRACE_IDENTITY_PROPERTY_NOT_FOUND;

    if (event_record == NULL ||
        property_name == NULL ||
        property_name[0] == L'\0' ||
        value == NULL ||
        expected_size == 0U ||
        expected_size > sizeof(GUID)) {
        return COMTRACE_IDENTITY_INVALID_ARGUMENT;
    }

    status = TdhGetEventInformation(
        (PEVENT_RECORD)event_record,
        0U,
        NULL,
        NULL,
        &buffer_size
    );

    if (status != ERROR_INSUFFICIENT_BUFFER ||
        buffer_size < property_offset ||
        buffer_size > COMTRACE_MAX_EVENT_INFO_SIZE) {
        return COMTRACE_IDENTITY_METADATA_UNAVAILABLE;
    }

    allocated_size = buffer_size;
    buffer = (BYTE *)malloc(buffer_size);
    if (buffer == NULL) {
        return COMTRACE_IDENTITY_METADATA_UNAVAILABLE;
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
        free(buffer);
        return COMTRACE_IDENTITY_METADATA_UNAVAILABLE;
    }

    event_info = (TRACE_EVENT_INFO *)buffer;
    available_count = (ULONG)(
        (buffer_size - property_offset) / sizeof(EVENT_PROPERTY_INFO)
    );

    if (event_info->PropertyCount > available_count ||
        event_info->TopLevelPropertyCount > event_info->PropertyCount) {
        free(buffer);
        return COMTRACE_IDENTITY_METADATA_UNAVAILABLE;
    }

    for (index = 0U; index < event_info->TopLevelPropertyCount; ++index) {
        const EVENT_PROPERTY_INFO *property =
            &event_info->EventPropertyInfoArray[index];
        ULONG name_offset = property->NameOffset;
        ULONG remaining;
        ULONG name_length = 0U;
        const wchar_t *name;
        PROPERTY_DATA_DESCRIPTOR descriptor;
        ULONG value_size = 0U;
        BYTE property_value[sizeof(GUID)];

        if (name_offset >= buffer_size ||
            (name_offset % (ULONG)sizeof(wchar_t)) != 0U) {
            result = COMTRACE_IDENTITY_METADATA_UNAVAILABLE;
            break;
        }

        name = (const wchar_t *)(buffer + name_offset);
        remaining = (buffer_size - name_offset) / (ULONG)sizeof(wchar_t);

        while (name_length < remaining && name[name_length] != L'\0') {
            ++name_length;
        }

        if (name_length == remaining) {
            result = COMTRACE_IDENTITY_METADATA_UNAVAILABLE;
            break;
        }

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
            property->nonStructType.InType != expected_type) {
            result = COMTRACE_IDENTITY_PROPERTY_UNSUPPORTED;
            break;
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

        if (status != ERROR_SUCCESS || value_size != expected_size) {
            result = COMTRACE_IDENTITY_VALUE_UNAVAILABLE;
            break;
        }

        status = TdhGetProperty(
            (PEVENT_RECORD)event_record,
            0U,
            NULL,
            1U,
            &descriptor,
            value_size,
            property_value
        );

        if (status != ERROR_SUCCESS) {
            result = COMTRACE_IDENTITY_VALUE_UNAVAILABLE;
            break;
        }

        CopyMemory(value, property_value, expected_size);
        result = COMTRACE_IDENTITY_SUCCESS;
        break;
    }

    free(buffer);
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

    if (status == COMTRACE_IDENTITY_SUCCESS) {
        *value = candidate;
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
    result.has_iid = status == COMTRACE_IDENTITY_SUCCESS;

    status = comtrace_extract_guid_property(event_record, L"IPID", &result.ipid);
    result.has_ipid = status == COMTRACE_IDENTITY_SUCCESS;

    status = comtrace_extract_uint64_property(event_record, L"OID", &result.oid);
    result.has_oid = status == COMTRACE_IDENTITY_SUCCESS;

    status = comtrace_extract_uint64_property(
        event_record,
        L"CallTraceId",
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
