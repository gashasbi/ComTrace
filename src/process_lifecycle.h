#ifndef COMTRACE_PROCESS_LIFECYCLE_H
#define COMTRACE_PROCESS_LIFECYCLE_H

#include <stddef.h>
#include <windows.h>
#include <evntcons.h>

typedef enum {
    COMTRACE_PROCESS_EVENT_UNKNOWN = 0,
    COMTRACE_PROCESS_EVENT_START,
    COMTRACE_PROCESS_EVENT_END,
    COMTRACE_PROCESS_EVENT_DC_START,
    COMTRACE_PROCESS_EVENT_DC_END,
    COMTRACE_PROCESS_EVENT_DEFUNCT
} COMTRACE_PROCESS_EVENT_TYPE;

typedef struct {
    DWORD process_id;
    DWORD parent_process_id;
    BOOL has_parent_process_id;
    LONGLONG timestamp;
    COMTRACE_PROCESS_EVENT_TYPE type;
} COMTRACE_PROCESS_EVENT;

typedef struct {
    DWORD process_id;
    ULONGLONG generation;
    DWORD parent_process_id;
    BOOL has_parent_process_id;
    LONGLONG first_seen_timestamp;
    BOOL has_first_seen_timestamp;
    BOOL started_during_trace;
    LONGLONG end_timestamp;
    BOOL has_end_timestamp;
    LONGLONG trace_end_timestamp;
    BOOL has_trace_end_timestamp;
} COMTRACE_PROCESS_LIFETIME;

typedef struct {
    COMTRACE_PROCESS_LIFETIME *items;
    size_t count;
    size_t capacity;
    ULONGLONG start_events;
    ULONGLONG end_events;
    ULONGLONG dc_start_events;
    ULONGLONG dc_end_events;
    ULONGLONG defunct_events;
} COMTRACE_PROCESS_LIFECYCLE_CATALOG;

typedef enum {
    COMTRACE_PROCESS_LIFECYCLE_SUCCESS = 0,
    COMTRACE_PROCESS_LIFECYCLE_INVALID_ARGUMENT,
    COMTRACE_PROCESS_LIFECYCLE_METADATA_UNAVAILABLE,
    COMTRACE_PROCESS_LIFECYCLE_ALLOCATION_FAILED,
    COMTRACE_PROCESS_LIFECYCLE_OVERFLOW
} COMTRACE_PROCESS_LIFECYCLE_STATUS;

BOOL comtrace_process_lifecycle_is_event(
    const EVENT_RECORD *event_record
);

COMTRACE_PROCESS_LIFECYCLE_STATUS comtrace_process_lifecycle_extract(
    const EVENT_RECORD *event_record,
    COMTRACE_PROCESS_EVENT *process_event
);

void comtrace_process_lifecycle_catalog_init(
    COMTRACE_PROCESS_LIFECYCLE_CATALOG *catalog
);

void comtrace_process_lifecycle_catalog_destroy(
    COMTRACE_PROCESS_LIFECYCLE_CATALOG *catalog
);

COMTRACE_PROCESS_LIFECYCLE_STATUS comtrace_process_lifecycle_catalog_add(
    COMTRACE_PROCESS_LIFECYCLE_CATALOG *catalog,
    const COMTRACE_PROCESS_EVENT *process_event
);

#endif
