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

typedef enum {
    COMTRACE_VALUE_INT8,
    COMTRACE_VALUE_UINT8,
    COMTRACE_VALUE_INT16,
    COMTRACE_VALUE_UINT16,
    COMTRACE_VALUE_INT32,
    COMTRACE_VALUE_UINT32,
    COMTRACE_VALUE_INT64,
    COMTRACE_VALUE_UINT64,
    COMTRACE_VALUE_FLOAT,
    COMTRACE_VALUE_DOUBLE,
    COMTRACE_VALUE_BOOLEAN,
    COMTRACE_VALUE_GUID,
    COMTRACE_VALUE_FILETIME,
    COMTRACE_VALUE_HEXINT32,
    COMTRACE_VALUE_HEXINT64,
    COMTRACE_VALUE_POINTER,
    COMTRACE_VALUE_SIZET
} ComTraceValueSizeKind;

int comtrace_value_size_is_valid(
    ComTraceValueSizeKind kind,
    size_t value_size,
    size_t pointer_size
);

#endif
