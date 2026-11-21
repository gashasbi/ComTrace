#ifndef COMTRACE_COM_TIMELINE_H
#define COMTRACE_COM_TIMELINE_H

#include <windows.h>
#include <stddef.h>
#include "com_identity.h"

typedef enum {
    COMTRACE_TIMELINE_SUCCESS = 0,
    COMTRACE_TIMELINE_INVALID_ARGUMENT,
    COMTRACE_TIMELINE_ALLOCATION_FAILED
} COMTRACE_TIMELINE_STATUS;

typedef enum {
    COMTRACE_TIMELINE_PHASE_START = 1,
    COMTRACE_TIMELINE_PHASE_STOP
} COMTRACE_TIMELINE_PHASE;

typedef enum {
    COMTRACE_TIMELINE_MATCH_NONE = 0,
    COMTRACE_TIMELINE_MATCH_CALL_TRACE_ID,
    COMTRACE_TIMELINE_MATCH_ACTIVITY_ID
} COMTRACE_TIMELINE_MATCH_TYPE;

typedef struct {
    const wchar_t *operation;
    COMTRACE_TIMELINE_PHASE phase;
    BOOL has_call_trace_id;
    ULONGLONG call_trace_id;
    DWORD process_id;
    DWORD thread_id;
    ULONGLONG timestamp;
    const COMTRACE_COM_OBJECT_IDENTITY *object_identity;
    const COMTRACE_ACTIVITY_IDS *activity_ids;
    BOOL has_hresult;
    HRESULT hresult;
} COMTRACE_TIMELINE_EVENT;

typedef struct {
    wchar_t operation[128];
    BOOL has_call_trace_id;
    ULONGLONG call_trace_id;
    DWORD process_id;
    DWORD thread_id;
    ULONGLONG start_timestamp;
    ULONGLONG stop_timestamp;
    ULONGLONG duration_ticks;
    BOOL has_start;
    BOOL has_stop;
    BOOL has_duration;
    COMTRACE_TIMELINE_MATCH_TYPE match_type;
    BOOL has_hresult;
    HRESULT hresult;
    BOOL has_object_identity;
    COMTRACE_COM_OBJECT_IDENTITY object_identity;
    BOOL has_activity_ids;
    COMTRACE_ACTIVITY_IDS activity_ids;
} COMTRACE_TIMELINE_RECORD;

typedef struct {
    COMTRACE_TIMELINE_RECORD *records;
    size_t record_count;
    size_t record_capacity;
    size_t *pending_indices;
    size_t pending_count;
    size_t pending_capacity;
    ULONGLONG missing_call_trace_ids;
    ULONGLONG missing_activity_ids;
    ULONGLONG unmatched_stops;
} COMTRACE_TIMELINE;

void comtrace_timeline_init(COMTRACE_TIMELINE *timeline);
void comtrace_timeline_destroy(COMTRACE_TIMELINE *timeline);

COMTRACE_TIMELINE_STATUS comtrace_timeline_add_event(
    COMTRACE_TIMELINE *timeline,
    const COMTRACE_TIMELINE_EVENT *event
);

size_t comtrace_timeline_count(const COMTRACE_TIMELINE *timeline);
size_t comtrace_timeline_pending_count(const COMTRACE_TIMELINE *timeline);

const COMTRACE_TIMELINE_RECORD *comtrace_timeline_get(
    const COMTRACE_TIMELINE *timeline,
    size_t index
);

#endif
