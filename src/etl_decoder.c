#include "etl_decoder.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

COMTRACE_ETL_EVENT_INFO_STATUS comtrace_etl_event_info_open(
    const EVENT_RECORD *event_record,
    COMTRACE_ETL_EVENT_INFO *event_info
)
{
    ULONG buffer_size = 0U;
    ULONG allocated_size;
    ULONG status;
    ULONG available_count;
    size_t property_offset = offsetof(TRACE_EVENT_INFO, EventPropertyInfoArray);

    if (event_info != NULL) {
        ZeroMemory(event_info, sizeof(*event_info));
    }
    if (event_record == NULL || event_info == NULL) {
        return COMTRACE_ETL_EVENT_INFO_INVALID_ARGUMENT;
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
        buffer_size > 65536U) {
        event_info->tdh_status = status;
        return COMTRACE_ETL_EVENT_INFO_UNAVAILABLE;
    }

    allocated_size = buffer_size;
    event_info->buffer = (BYTE *)malloc(buffer_size);
    if (event_info->buffer == NULL) {
        return COMTRACE_ETL_EVENT_INFO_ALLOCATION_FAILED;
    }

    status = TdhGetEventInformation(
        (PEVENT_RECORD)event_record,
        0U,
        NULL,
        (PTRACE_EVENT_INFO)event_info->buffer,
        &buffer_size
    );
    if (status != ERROR_SUCCESS ||
        buffer_size < property_offset ||
        buffer_size > allocated_size) {
        event_info->tdh_status = status;
        comtrace_etl_event_info_close(event_info);
        return COMTRACE_ETL_EVENT_INFO_UNAVAILABLE;
    }

    event_info->size = buffer_size;
    event_info->info = (TRACE_EVENT_INFO *)event_info->buffer;
    available_count = (ULONG)(
        (buffer_size - property_offset) / sizeof(EVENT_PROPERTY_INFO)
    );
    if (event_info->info->PropertyCount > available_count ||
        event_info->info->TopLevelPropertyCount > event_info->info->PropertyCount) {
        comtrace_etl_event_info_close(event_info);
        return COMTRACE_ETL_EVENT_INFO_INVALID_METADATA;
    }

    event_info->property_count = event_info->info->PropertyCount;
    event_info->top_level_property_count = event_info->info->TopLevelPropertyCount;
    return COMTRACE_ETL_EVENT_INFO_SUCCESS;
}

void comtrace_etl_event_info_close(
    COMTRACE_ETL_EVENT_INFO *event_info
)
{
    if (event_info == NULL) {
        return;
    }

    free(event_info->buffer);
    ZeroMemory(event_info, sizeof(*event_info));
}

COMTRACE_ETL_PROPERTY_READ_STATUS comtrace_etl_read_property_value(
    const EVENT_RECORD *event_record,
    const EVENT_PROPERTY_INFO *property,
    const wchar_t *name,
    const wchar_t *const *parent_names,
    const ULONG *parent_indices,
    ULONG parent_count,
    ULONG array_index,
    COMTRACE_ETL_PROPERTY_VALUE *value
)
{
    PROPERTY_DATA_DESCRIPTOR descriptors[COMTRACE_ETL_MAX_PROPERTY_DEPTH + 1U];
    ULONG descriptor_count;
    ULONG index;
    ULONG status;

    if (value != NULL) {
        ZeroMemory(value, sizeof(*value));
    }

    if (event_record == NULL ||
        property == NULL ||
        name == NULL ||
        value == NULL ||
        parent_count > COMTRACE_ETL_MAX_PROPERTY_DEPTH ||
        (parent_count != 0U &&
            (parent_names == NULL || parent_indices == NULL))) {
        return COMTRACE_ETL_PROPERTY_READ_INVALID_ARGUMENT;
    }

    if ((property->Flags & PropertyStruct) != 0U) {
        return COMTRACE_ETL_PROPERTY_READ_UNSUPPORTED;
    }

    ZeroMemory(descriptors, sizeof(descriptors));
    for (index = 0U; index < parent_count; ++index) {
        if (parent_names[index] == NULL) {
            return COMTRACE_ETL_PROPERTY_READ_INVALID_ARGUMENT;
        }
        descriptors[index].PropertyName =
            (ULONGLONG)(ULONG_PTR)parent_names[index];
        descriptors[index].ArrayIndex = parent_indices[index];
    }

    descriptors[parent_count].PropertyName = (ULONGLONG)(ULONG_PTR)name;
    descriptors[parent_count].ArrayIndex = array_index;
    descriptor_count = parent_count + 1U;

    status = TdhGetPropertySize(
        (PEVENT_RECORD)event_record,
        0U,
        NULL,
        descriptor_count,
        descriptors,
        &value->size
    );

    if (status != ERROR_SUCCESS) {
        value->tdh_status = status;
        return COMTRACE_ETL_PROPERTY_READ_SIZE_QUERY_FAILED;
    }

    if (value->size == 0U) {
        return COMTRACE_ETL_PROPERTY_READ_ZERO_SIZE;
    }
    if (value->size > COMTRACE_ETL_MAX_PROPERTY_BYTES) {
        value->tdh_status = ERROR_FILE_TOO_LARGE;
        return COMTRACE_ETL_PROPERTY_READ_SIZE_LIMIT;
    }

    value->data = (BYTE *)malloc(value->size);
    if (value->data == NULL) {
        value->size = 0U;
        return COMTRACE_ETL_PROPERTY_READ_ALLOCATION_FAILED;
    }

    status = TdhGetProperty(
        (PEVENT_RECORD)event_record,
        0U,
        NULL,
        descriptor_count,
        descriptors,
        value->size,
        value->data
    );

    if (status != ERROR_SUCCESS) {
        value->tdh_status = status;
        free(value->data);
        value->data = NULL;
        value->size = 0U;
        return COMTRACE_ETL_PROPERTY_READ_FAILED;
    }

    return COMTRACE_ETL_PROPERTY_READ_SUCCESS;
}

void comtrace_etl_property_value_destroy(
    COMTRACE_ETL_PROPERTY_VALUE *value
)
{
    if (value == NULL) {
        return;
    }

    free(value->data);
    ZeroMemory(value, sizeof(*value));
}

int comtrace_etl_read_property_count(
    const EVENT_RECORD *event_record,
    const EVENT_PROPERTY_INFO *properties,
    ULONG property_count,
    const BYTE *metadata_buffer,
    ULONG metadata_size,
    const EVENT_PROPERTY_INFO *property,
    const wchar_t *const *parent_names,
    const ULONG *parent_indices,
    const ULONG *parent_property_indices,
    ULONG parent_count,
    ULONG *count
)
{
    PROPERTY_DATA_DESCRIPTOR descriptors[COMTRACE_ETL_MAX_PROPERTY_DEPTH + 1U];
    const EVENT_PROPERTY_INFO *count_property;
    const wchar_t *count_name;
    ULONG count_index;
    ULONG descriptor_parent_count = 0U;
    ULONG descriptor_count;
    ULONG index;
    ULONG name_offset;
    ULONG remaining;
    ULONG name_length = 0U;
    ULONG value_size = 0U;
    ULONG expected_size;
    ULONG status;
    USHORT in_type;
    BYTE value[8];
    LONGLONG signed_value = 0LL;
    ULONGLONG count_value = 0ULL;

    if (event_record == NULL ||
        properties == NULL ||
        metadata_buffer == NULL ||
        property == NULL ||
        count == NULL ||
        parent_count > COMTRACE_ETL_MAX_PROPERTY_DEPTH ||
        (parent_count != 0U &&
            (parent_names == NULL ||
             parent_indices == NULL ||
             parent_property_indices == NULL))) {
        return 0;
    }

    count_index = (ULONG)property->countPropertyIndex;
    if (count_index >= property_count) {
        return 0;
    }

    count_property = &properties[count_index];
    if ((count_property->Flags & PropertyStruct) != 0U) {
        return 0;
    }

    name_offset = count_property->NameOffset;
    if (name_offset >= metadata_size ||
        (name_offset % (ULONG)sizeof(wchar_t)) != 0U) {
        return 0;
    }

    count_name = (const wchar_t *)(metadata_buffer + name_offset);
    remaining = (metadata_size - name_offset) / (ULONG)sizeof(wchar_t);
    while (name_length < remaining && count_name[name_length] != L'\0') {
        ++name_length;
    }
    if (name_length == remaining) {
        return 0;
    }

    in_type = (count_property->Flags & PropertyHasCustomSchema) != 0U
        ? count_property->customSchemaType.InType
        : count_property->nonStructType.InType;
    switch (in_type) {
    case TDH_INTYPE_INT8:
    case TDH_INTYPE_UINT8:
        expected_size = 1U;
        break;
    case TDH_INTYPE_INT16:
    case TDH_INTYPE_UINT16:
        expected_size = 2U;
        break;
    case TDH_INTYPE_INT32:
    case TDH_INTYPE_UINT32:
    case TDH_INTYPE_HEXINT32:
        expected_size = 4U;
        break;
    case TDH_INTYPE_INT64:
    case TDH_INTYPE_UINT64:
    case TDH_INTYPE_HEXINT64:
        expected_size = 8U;
        break;
    default:
        return 0;
    }

    ZeroMemory(descriptors, sizeof(descriptors));
    for (index = 0U; index < parent_count; ++index) {
        ULONG parent_property_index = parent_property_indices[index];
        const EVENT_PROPERTY_INFO *parent_property;
        ULONG member_start;
        ULONG member_count;

        if (parent_names[index] == NULL ||
            parent_property_index >= property_count) {
            return 0;
        }

        parent_property = &properties[parent_property_index];
        if ((parent_property->Flags & PropertyStruct) == 0U) {
            return 0;
        }

        member_start = parent_property->structType.StructStartIndex;
        member_count = parent_property->structType.NumOfStructMembers;
        if (count_index >= member_start &&
            count_index - member_start < member_count) {
            descriptor_parent_count = index + 1U;
        }
    }

    for (index = 0U; index < descriptor_parent_count; ++index) {
        descriptors[index].PropertyName =
            (ULONGLONG)(ULONG_PTR)parent_names[index];
        descriptors[index].ArrayIndex = parent_indices[index];
    }

    descriptors[descriptor_parent_count].PropertyName =
        (ULONGLONG)(ULONG_PTR)count_name;
    descriptors[descriptor_parent_count].ArrayIndex = 0xFFFFFFFFUL;
    descriptor_count = descriptor_parent_count + 1U;

    status = TdhGetPropertySize(
        (PEVENT_RECORD)event_record,
        0U,
        NULL,
        descriptor_count,
        descriptors,
        &value_size
    );
    if (status != ERROR_SUCCESS || value_size != expected_size) {
        return 0;
    }

    status = TdhGetProperty(
        (PEVENT_RECORD)event_record,
        0U,
        NULL,
        descriptor_count,
        descriptors,
        value_size,
        value
    );
    if (status != ERROR_SUCCESS) {
        return 0;
    }

    switch (in_type) {
    case TDH_INTYPE_INT8: {
        signed char number;
        CopyMemory(&number, value, sizeof(number));
        signed_value = (LONGLONG)number;
        break;
    }
    case TDH_INTYPE_INT16: {
        SHORT number;
        CopyMemory(&number, value, sizeof(number));
        signed_value = (LONGLONG)number;
        break;
    }
    case TDH_INTYPE_INT32: {
        LONG number;
        CopyMemory(&number, value, sizeof(number));
        signed_value = (LONGLONG)number;
        break;
    }
    case TDH_INTYPE_INT64:
        CopyMemory(&signed_value, value, sizeof(signed_value));
        break;
    default:
        CopyMemory(&count_value, value, value_size);
        break;
    }

    if (in_type == TDH_INTYPE_INT8 ||
        in_type == TDH_INTYPE_INT16 ||
        in_type == TDH_INTYPE_INT32 ||
        in_type == TDH_INTYPE_INT64) {
        if (signed_value < 0LL) {
            return 0;
        }
        count_value = (ULONGLONG)signed_value;
    }

    if (count_value > (ULONGLONG)COMTRACE_ETL_MAX_ARRAY_ELEMENTS) {
        return 0;
    }

    *count = (ULONG)count_value;
    return 1;
}
