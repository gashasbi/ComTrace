#ifndef COMTRACE_OLE_TIMELINE_H
#define COMTRACE_OLE_TIMELINE_H

#include <stddef.h>
#include "com_identity.h"

typedef enum {
    COMTRACE_OLE_TIMELINE_SUCCESS = 0,
    COMTRACE_OLE_TIMELINE_INVALID_ARGUMENT,
    COMTRACE_OLE_TIMELINE_ALLOCATION_FAILED
} COMTRACE_OLE_TIMELINE_STATUS;

typedef enum {
    COMTRACE_OLE_TIMELINE_PHASE_START = 1,
    COMTRACE_OLE_TIMELINE_PHASE_STOP
} COMTRACE_OLE_TIMELINE_PHASE;

typedef enum {
    COMTRACE_OLE_TIMELINE_MATCH_NONE = 0,
    COMTRACE_OLE_TIMELINE_MATCH_ACTIVITY_ID,
    COMTRACE_OLE_TIMELINE_MATCH_PROCESS_THREAD,
    COMTRACE_OLE_TIMELINE_MATCH_RELATED_ACTIVITY_ID
} COMTRACE_OLE_TIMELINE_MATCH_TYPE;

typedef struct {
    const wchar_t *operation;
    COMTRACE_OLE_TIMELINE_PHASE phase;
    DWORD process_id;
    DWORD thread_id;
    ULONGLONG timestamp;
    const COMTRACE_ACTIVITY_IDS *activity_ids;
} COMTRACE_OLE_TIMELINE_EVENT;

typedef struct {
    wchar_t operation[128];
    DWORD process_id;
    DWORD thread_id;
    DWORD stop_process_id;
    DWORD stop_thread_id;
    ULONGLONG start_timestamp;
    ULONGLONG stop_timestamp;
    ULONGLONG duration_ticks;
    BOOL has_start;
    BOOL has_stop;
    BOOL has_duration;
    BOOL invalid_duration_order;
    BOOL ambiguous_match;
    COMTRACE_OLE_TIMELINE_MATCH_TYPE match_type;
    BOOL has_activity_id;
    GUID activity_id;
    BOOL has_related_activity_id;
    GUID related_activity_id;
} COMTRACE_OLE_TIMELINE_RECORD;

typedef struct {
    COMTRACE_OLE_TIMELINE_RECORD *records;
    size_t record_count;
    size_t record_capacity;
    size_t *pending_indices;
    size_t pending_count;
    size_t pending_capacity;
    ULONGLONG unmatched_stops;
    ULONGLONG ambiguous_stops;
    ULONGLONG invalid_duration_orders;
    ULONGLONG missing_activity_ids;
} COMTRACE_OLE_TIMELINE;

void comtrace_ole_timeline_init(COMTRACE_OLE_TIMELINE *timeline);
void comtrace_ole_timeline_destroy(COMTRACE_OLE_TIMELINE *timeline);

COMTRACE_OLE_TIMELINE_STATUS comtrace_ole_timeline_add_event(
    COMTRACE_OLE_TIMELINE *timeline,
    const COMTRACE_OLE_TIMELINE_EVENT *event
);

size_t comtrace_ole_timeline_count(const COMTRACE_OLE_TIMELINE *timeline);
size_t comtrace_ole_timeline_pending_count(const COMTRACE_OLE_TIMELINE *timeline);
size_t comtrace_ole_timeline_sequence_count(const COMTRACE_OLE_TIMELINE *timeline);

const COMTRACE_OLE_TIMELINE_RECORD *comtrace_ole_timeline_get(
    const COMTRACE_OLE_TIMELINE *timeline,
    size_t index
);

#endif
