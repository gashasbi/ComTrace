#ifndef COMTRACE_ETL_DECODER_H
#define COMTRACE_ETL_DECODER_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <evntcons.h>
#include <tdh.h>

typedef enum {
    COMTRACE_ETL_PROPERTY_READ_SUCCESS = 0,
    COMTRACE_ETL_PROPERTY_READ_INVALID_ARGUMENT,
    COMTRACE_ETL_PROPERTY_READ_UNSUPPORTED,
    COMTRACE_ETL_PROPERTY_READ_SIZE_QUERY_FAILED,
    COMTRACE_ETL_PROPERTY_READ_ZERO_SIZE,
    COMTRACE_ETL_PROPERTY_READ_ALLOCATION_FAILED,
    COMTRACE_ETL_PROPERTY_READ_FAILED
} COMTRACE_ETL_PROPERTY_READ_STATUS;

typedef struct {
    BYTE *data;
    ULONG size;
    ULONG tdh_status;
} COMTRACE_ETL_PROPERTY_VALUE;

COMTRACE_ETL_PROPERTY_READ_STATUS comtrace_etl_read_property_value(
    const EVENT_RECORD *event_record,
    const EVENT_PROPERTY_INFO *property,
    const wchar_t *name,
    const wchar_t *const *parent_names,
    const ULONG *parent_indices,
    ULONG parent_count,
    ULONG array_index,
    COMTRACE_ETL_PROPERTY_VALUE *value
);

void comtrace_etl_property_value_destroy(
    COMTRACE_ETL_PROPERTY_VALUE *value
);

int comtrace_etl_read_property_count(
    const EVENT_RECORD *event_record,
    const EVENT_PROPERTY_INFO *properties,
    ULONG property_count,
    const BYTE *metadata_buffer,
    ULONG metadata_size,
    const EVENT_PROPERTY_INFO *property,
    ULONG *count
);

#endif
