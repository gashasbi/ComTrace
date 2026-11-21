#ifndef COMTRACE_TRACE_COMPARE_H
#define COMTRACE_TRACE_COMPARE_H

#include "event_catalog.h"
#include "relationship_graph.h"
#include "com_timeline.h"
#include "ole_timeline.h"
#include "object_identity_compare.h"

typedef enum {
    COMTRACE_TRACE_COMPARE_SUCCESS = 0,
    COMTRACE_TRACE_COMPARE_INVALID_ARGUMENT,
    COMTRACE_TRACE_COMPARE_UNSUPPORTED_FORMAT,
    COMTRACE_TRACE_COMPARE_ETL_OPEN_FAILED,
    COMTRACE_TRACE_COMPARE_ETL_PROCESS_FAILED,
    COMTRACE_TRACE_COMPARE_ARCHIVE_FAILED,
    COMTRACE_TRACE_COMPARE_CATALOG_FAILED,
    COMTRACE_TRACE_COMPARE_ALLOCATION_FAILED,
    COMTRACE_TRACE_COMPARE_COUNT_OVERFLOW
} COMTRACE_TRACE_COMPARE_STATUS;

typedef struct {
    GUID provider_id;
    EVENT_DESCRIPTOR descriptor;
    ULONGLONG left_count;
    ULONGLONG right_count;
} COMTRACE_EVENT_TYPE_DIFF;

typedef struct {
    DWORD process_id;
    ULONGLONG object_id;
    BOOL present_in_left;
    BOOL present_in_right;
    ULONG changed_fields;
} COMTRACE_OBJECT_DIFF;

typedef struct {
    wchar_t operation[128];
    ULONGLONG call_count;
    ULONGLONG failed_count;
    double average_duration_ms;
    double maximum_duration_ms;
} COMTRACE_OPERATION_TIMING;

typedef struct {
    COMTRACE_EVENT_CATALOG event_catalog;
    COMTRACE_RELATIONSHIP_GRAPH relationship_graph;
    COMTRACE_TIMELINE com_timeline;
    COMTRACE_OLE_TIMELINE ole_timeline;
    COMTRACE_OPERATION_TIMING *operation_timings;
    size_t operation_timing_count;
    size_t operation_timing_capacity;
    ULONGLONG total_events;
    ULONGLONG trace_frequency;
} COMTRACE_TRACE_SUMMARY;

typedef struct {
    COMTRACE_TRACE_SUMMARY left;
    COMTRACE_TRACE_SUMMARY right;
    COMTRACE_EVENT_TYPE_DIFF *event_type_diffs;
    size_t event_type_diff_count;
    COMTRACE_OBJECT_DIFF *object_diffs;
    size_t object_diff_count;
    ULONGLONG added_event_types;
    ULONGLONG removed_event_types;
    ULONGLONG changed_event_types;
    ULONGLONG added_objects;
    ULONGLONG removed_objects;
    ULONGLONG changed_objects;
} COMTRACE_TRACE_COMPARISON;

COMTRACE_TRACE_COMPARE_STATUS comtrace_trace_compare(
    const wchar_t *left_path,
    const wchar_t *right_path,
    COMTRACE_TRACE_COMPARISON *comparison
);

void comtrace_trace_comparison_destroy(
    COMTRACE_TRACE_COMPARISON *comparison
);

void comtrace_trace_comparison_write_text(
    const COMTRACE_TRACE_COMPARISON *comparison
);

void comtrace_trace_comparison_write_json(
    const COMTRACE_TRACE_COMPARISON *comparison,
    const wchar_t *left_path,
    const wchar_t *right_path
);

#endif
