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