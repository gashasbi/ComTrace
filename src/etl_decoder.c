#include "etl_decoder.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

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
    PROPERTY_DATA_DESCRIPTOR descriptors[9];
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
        parent_count > 8U ||
        (parent_count != 0U &&
            (parent_names == NULL || parent_indices == NULL))) {
        return COMTRACE_ETL_PROPERTY_READ_INVALID_ARGUMENT;
    }

    if ((property->Flags & (
            PropertyStruct |
            PropertyWBEMXmlFragment |
            PropertyHasCustomSchema
        )) != 0U) {
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
    ULONG *count
)
{
    PROPERTY_DATA_DESCRIPTOR descriptor;
    const EVENT_PROPERTY_INFO *count_property;
    const wchar_t *count_name;
    ULONG count_index;
    ULONG name_offset;
    ULONG remaining;
    ULONG name_length = 0U;
    ULONG value_size = 0U;
    ULONG expected_size;
    ULONG status;
    USHORT in_type;
    BYTE value[8];
    ULONGLONG count_value = 0ULL;

    if (event_record == NULL ||
        properties == NULL ||
        metadata_buffer == NULL ||
        property == NULL ||
        count == NULL) {
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

    in_type = count_property->nonStructType.InType;
    switch (in_type) {
    case TDH_INTYPE_UINT8:
        expected_size = 1U;
        break;
    case TDH_INTYPE_UINT16:
        expected_size = 2U;
        break;
    case TDH_INTYPE_UINT32:
        expected_size = 4U;
        break;
    case TDH_INTYPE_UINT64:
        expected_size = 8U;
        break;
    default:
        return 0;
    }

    ZeroMemory(&descriptor, sizeof(descriptor));
    descriptor.PropertyName = (ULONGLONG)(ULONG_PTR)count_name;
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
        return 0;
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
        return 0;
    }

    CopyMemory(&count_value, value, value_size);
    if (count_value > 4096ULL) {
        return 0;
    }

    *count = (ULONG)count_value;
    return 1;
}
