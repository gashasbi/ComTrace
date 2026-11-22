#include "etl_value.h"

int comtrace_scan_string(
    const unsigned char *value,
    size_t value_size,
    size_t unit_size,
    size_t display_limit,
    size_t *content_size,
    size_t *displayed_size,
    int *terminated
)
{
    size_t offset;
    size_t limit;

    if (value == NULL || content_size == NULL ||
        displayed_size == NULL || terminated == NULL ||
        (unit_size != 1U && unit_size != 2U) ||
        value_size % unit_size != 0U) {
        return 0;
    }

    limit = display_limit - (display_limit % unit_size);
    *content_size = value_size;
    *terminated = 0;

    for (offset = 0U; offset < value_size; offset += unit_size) {
        if (value[offset] == 0U &&
            (unit_size == 1U || value[offset + 1U] == 0U)) {
            *content_size = offset;
            *terminated = 1;
            break;
        }
    }

    *displayed_size = *content_size < limit
        ? *content_size
        : limit;

    return 1;
}

int comtrace_value_size_is_valid(
    ComTraceValueSizeKind kind,
    size_t value_size,
    size_t pointer_size
)
{
    switch (kind) {
    case COMTRACE_VALUE_INT8:
    case COMTRACE_VALUE_UINT8:
    case COMTRACE_VALUE_BOOLEAN:
        return value_size == 1U ||
            (kind == COMTRACE_VALUE_BOOLEAN &&
             (value_size == 2U || value_size == 4U || value_size == 8U));
    case COMTRACE_VALUE_INT16:
    case COMTRACE_VALUE_UINT16:
        return value_size == 2U;
    case COMTRACE_VALUE_INT32:
    case COMTRACE_VALUE_UINT32:
    case COMTRACE_VALUE_FLOAT:
    case COMTRACE_VALUE_HEXINT32:
        return value_size == 4U;
    case COMTRACE_VALUE_INT64:
    case COMTRACE_VALUE_UINT64:
    case COMTRACE_VALUE_DOUBLE:
    case COMTRACE_VALUE_FILETIME:
    case COMTRACE_VALUE_HEXINT64:
        return value_size == 8U;
    case COMTRACE_VALUE_GUID:
    case COMTRACE_VALUE_SYSTEMTIME:
        return value_size == 16U;
    case COMTRACE_VALUE_POINTER:
    case COMTRACE_VALUE_SIZET:
        return pointer_size != 0U && value_size == pointer_size;
    default:
        return 0;
    }
}

static uint16_t read_uint16_le(const unsigned char *value)
{
    return (uint16_t)((uint16_t)value[0] | ((uint16_t)value[1] << 8U));
}

int comtrace_decode_systemtime(const unsigned char *value, size_t value_size, ComTraceSystemTime *system_time)
{
    ComTraceSystemTime decoded;

    if (value == NULL || system_time == NULL ||
        !comtrace_value_size_is_valid(COMTRACE_VALUE_SYSTEMTIME, value_size, 0U)) {
        return 0;
    }

    decoded.year = read_uint16_le(value);
    decoded.month = read_uint16_le(value + 2U);
    decoded.day_of_week = read_uint16_le(value + 4U);
    decoded.day = read_uint16_le(value + 6U);
    decoded.hour = read_uint16_le(value + 8U);
    decoded.minute = read_uint16_le(value + 10U);
    decoded.second = read_uint16_le(value + 12U);
    decoded.milliseconds = read_uint16_le(value + 14U);

    *system_time = decoded;
    return 1;
}


int comtrace_decode_length_prefixed_payload(
    const unsigned char *value,
    size_t value_size,
    size_t prefix_size,
    int reverse_order,
    size_t *payload_offset,
    size_t *payload_size
)
{
    size_t index;
    size_t decoded_length = 0U;

    if (value == NULL ||
        payload_offset == NULL ||
        payload_size == NULL ||
        (prefix_size != 2U && prefix_size != 4U) ||
        value_size < prefix_size) {
        return 0;
    }

    for (index = 0U; index < prefix_size; ++index) {
        size_t shift = reverse_order
            ? (prefix_size - index - 1U) * 8U
            : index * 8U;

        decoded_length |= (size_t)value[index] << shift;
    }

    if (decoded_length > value_size - prefix_size) {
        return 0;
    }

    *payload_offset = prefix_size;
    *payload_size = decoded_length;
    return 1;
}

int comtrace_decode_wbem_sid_payload(
    size_t value_size,
    size_t pointer_size,
    size_t *payload_offset,
    size_t *payload_size
)
{
    size_t offset;

    if (payload_offset == NULL ||
        payload_size == NULL ||
        (pointer_size != 4U && pointer_size != 8U)) {
        return 0;
    }

    offset = pointer_size * 2U;
    if (value_size <= offset) {
        return 0;
    }

    *payload_offset = offset;
    *payload_size = value_size - offset;
    return 1;
}
