#ifndef COMTRACE_ETL_VALUE_H
#define COMTRACE_ETL_VALUE_H

#include <stddef.h>
#include <stdint.h>
typedef struct {
    uint16_t year;
    uint16_t month;
    uint16_t day_of_week;
    uint16_t day;
    uint16_t hour;
    uint16_t minute;
    uint16_t second;
    uint16_t milliseconds;
} ComTraceSystemTime;

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
    COMTRACE_VALUE_SIZET,
    COMTRACE_VALUE_SYSTEMTIME
} ComTraceValueSizeKind;

int comtrace_value_size_is_valid(
    ComTraceValueSizeKind kind,
    size_t value_size,
    size_t pointer_size
);

int comtrace_decode_systemtime(const unsigned char *value, size_t value_size, ComTraceSystemTime *system_time);

int comtrace_decode_length_prefixed_payload(
    const unsigned char *value,
    size_t value_size,
    size_t prefix_size,
    int reverse_order,
    size_t *payload_offset,
    size_t *payload_size
);

int comtrace_decode_wbem_sid_payload(
    size_t value_size,
    size_t pointer_size,
    size_t *payload_offset,
    size_t *payload_size
);

#endif
