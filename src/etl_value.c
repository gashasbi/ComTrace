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
        return value_size == 16U;
    case COMTRACE_VALUE_POINTER:
    case COMTRACE_VALUE_SIZET:
        return pointer_size != 0U && value_size == pointer_size;
    default:
        return 0;
    }
}
