#ifndef COMTRACE_ETL_DECODER_H
#define COMTRACE_ETL_DECODER_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <evntcons.h>
#include <tdh.h>

#define COMTRACE_ETL_MAX_PROPERTY_DEPTH 16U
#define COMTRACE_ETL_MAX_ARRAY_ELEMENTS 4096U
#define COMTRACE_ETL_MAX_PROPERTY_BYTES (16U * 1024U * 1024U)

typedef enum {
    COMTRACE_ETL_PROPERTY_READ_SUCCESS = 0,
    COMTRACE_ETL_PROPERTY_READ_INVALID_ARGUMENT,
    COMTRACE_ETL_PROPERTY_READ_UNSUPPORTED,
    COMTRACE_ETL_PROPERTY_READ_SIZE_QUERY_FAILED,
    COMTRACE_ETL_PROPERTY_READ_ZERO_SIZE,
    COMTRACE_ETL_PROPERTY_READ_SIZE_LIMIT,
    COMTRACE_ETL_PROPERTY_READ_ALLOCATION_FAILED,
    COMTRACE_ETL_PROPERTY_READ_FAILED
} COMTRACE_ETL_PROPERTY_READ_STATUS;

typedef enum {
    COMTRACE_ETL_EVENT_INFO_SUCCESS = 0,
    COMTRACE_ETL_EVENT_INFO_INVALID_ARGUMENT,
    COMTRACE_ETL_EVENT_INFO_UNAVAILABLE,
    COMTRACE_ETL_EVENT_INFO_ALLOCATION_FAILED,
    COMTRACE_ETL_EVENT_INFO_INVALID_METADATA
} COMTRACE_ETL_EVENT_INFO_STATUS;

typedef struct {
    BYTE *buffer;
    ULONG size;
    TRACE_EVENT_INFO *info;
    ULONG property_count;
    ULONG top_level_property_count;
    ULONG tdh_status;
} COMTRACE_ETL_EVENT_INFO;

typedef struct {
    BYTE *data;
    ULONG size;
    ULONG tdh_status;
} COMTRACE_ETL_PROPERTY_VALUE;

COMTRACE_ETL_EVENT_INFO_STATUS comtrace_etl_event_info_open(
    const EVENT_RECORD *event_record,
    COMTRACE_ETL_EVENT_INFO *event_info
);

void comtrace_etl_event_info_close(
    COMTRACE_ETL_EVENT_INFO *event_info
);

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
    const wchar_t *const *parent_names,
    const ULONG *parent_indices,
    const ULONG *parent_property_indices,
    ULONG parent_count,
    ULONG *count
);

#endif
