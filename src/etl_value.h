#ifndef COMTRACE_ETL_VALUE_H
#define COMTRACE_ETL_VALUE_H

#include <stddef.h>

int comtrace_scan_string(
    const unsigned char *value,
    size_t value_size,
    size_t unit_size,
    size_t display_limit,
    size_t *content_size,
    size_t *displayed_size,
    int *terminated
);

#endif