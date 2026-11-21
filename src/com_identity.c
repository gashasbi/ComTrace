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
