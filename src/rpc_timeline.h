#ifndef COMTRACE_RPC_TIMELINE_H
#define COMTRACE_RPC_TIMELINE_H

#include <stddef.h>
#include "com_identity.h"

typedef enum {
    COMTRACE_RPC_TIMELINE_SUCCESS = 0,
    COMTRACE_RPC_TIMELINE_INVALID_ARGUMENT,
    COMTRACE_RPC_TIMELINE_ALLOCATION_FAILED
} COMTRACE_RPC_TIMELINE_STATUS;

typedef enum {
    COMTRACE_RPC_TIMELINE_PHASE_START = 1,
    COMTRACE_RPC_TIMELINE_PHASE_STOP
} COMTRACE_RPC_TIMELINE_PHASE;

typedef enum {
    COMTRACE_RPC_TIMELINE_MATCH_NONE = 0,
    COMTRACE_RPC_TIMELINE_MATCH_ACTIVITY_ID,
    COMTRACE_RPC_TIMELINE_MATCH_PROCESS_THREAD
} COMTRACE_RPC_TIMELINE_MATCH_TYPE;

typedef struct {
    const wchar_t *operation;
    COMTRACE_RPC_TIMELINE_PHASE phase;
    DWORD process_id;
    DWORD thread_id;
    ULONGLONG timestamp;
    const COMTRACE_ACTIVITY_IDS *activity_ids;
    BOOL has_interface_uuid;
    GUID interface_uuid;
    BOOL has_procedure_number;
    ULONG procedure_number;
} COMTRACE_RPC_TIMELINE_EVENT;

typedef struct {
    wchar_t operation[64];
    DWORD process_id;
    DWORD thread_id;
    ULONGLONG start_timestamp;
    ULONGLONG stop_timestamp;
    ULONGLONG duration_ticks;
    BOOL has_start;
    BOOL has_stop;
    BOOL has_duration;
    COMTRACE_RPC_TIMELINE_MATCH_TYPE match_type;
    BOOL has_activity_id;
    GUID activity_id;
    BOOL has_related_activity_id;
    GUID related_activity_id;
    BOOL has_interface_uuid;
    GUID interface_uuid;
    BOOL has_procedure_number;
    ULONG procedure_number;
} COMTRACE_RPC_TIMELINE_RECORD;

typedef struct {
    COMTRACE_RPC_TIMELINE_RECORD *records;
    size_t record_count;
    size_t record_capacity;
    size_t *pending_indices;
    size_t pending_count;
    size_t pending_capacity;
    ULONGLONG unmatched_stops;
    ULONGLONG missing_activity_ids;
} COMTRACE_RPC_TIMELINE;

void comtrace_rpc_timeline_init(COMTRACE_RPC_TIMELINE *timeline);
void comtrace_rpc_timeline_destroy(COMTRACE_RPC_TIMELINE *timeline);

COMTRACE_RPC_TIMELINE_STATUS comtrace_rpc_timeline_add_event(
    COMTRACE_RPC_TIMELINE *timeline,
    const COMTRACE_RPC_TIMELINE_EVENT *event
);

size_t comtrace_rpc_timeline_count(const COMTRACE_RPC_TIMELINE *timeline);
size_t comtrace_rpc_timeline_pending_count(const COMTRACE_RPC_TIMELINE *timeline);

const COMTRACE_RPC_TIMELINE_RECORD *comtrace_rpc_timeline_get(
    const COMTRACE_RPC_TIMELINE *timeline,
    size_t index
);

#endif
