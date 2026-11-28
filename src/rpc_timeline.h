#ifndef COMTRACE_RPC_TIMELINE_H
#define COMTRACE_RPC_TIMELINE_H

#include <stddef.h>
#include "com_identity.h"
#include "com_timeline.h"

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
    COMTRACE_RPC_TIMELINE_MATCH_RELATED_ACTIVITY_ID,
    COMTRACE_RPC_TIMELINE_MATCH_PROCESS_THREAD
} COMTRACE_RPC_TIMELINE_MATCH_TYPE;

typedef enum {
    COMTRACE_RPC_ROLE_UNKNOWN = 0,
    COMTRACE_RPC_ROLE_CLIENT,
    COMTRACE_RPC_ROLE_SERVER,
    COMTRACE_RPC_ROLE_FUNCTION
} COMTRACE_RPC_ROLE;

typedef enum {
    COMTRACE_RPC_LOCATION_UNKNOWN = 0,
    COMTRACE_RPC_LOCATION_LOCAL,
    COMTRACE_RPC_LOCATION_NETWORK
} COMTRACE_RPC_LOCATION;

typedef enum {
    COMTRACE_RPC_CORRELATION_NONE = 0,
    COMTRACE_RPC_CORRELATION_ACTIVITY_ID,
    COMTRACE_RPC_CORRELATION_RELATED_ACTIVITY_ID
} COMTRACE_RPC_CORRELATION_TYPE;

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
    const wchar_t *protocol;
    const wchar_t *network_address;
    const wchar_t *endpoint;
    BOOL has_status;
    ULONG status;
} COMTRACE_RPC_TIMELINE_EVENT;

typedef struct {
    wchar_t operation[64];
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
    BOOL has_status;
    ULONG status;
    BOOL status_is_failure;
    COMTRACE_RPC_TIMELINE_MATCH_TYPE match_type;
    BOOL has_activity_id;
    GUID activity_id;
    BOOL has_related_activity_id;
    GUID related_activity_id;
    BOOL has_interface_uuid;
    GUID interface_uuid;
    BOOL has_procedure_number;
    ULONG procedure_number;
    COMTRACE_RPC_ROLE role;
    COMTRACE_RPC_LOCATION location;
    wchar_t protocol[64];
    wchar_t network_address[128];
    wchar_t endpoint[128];
    BOOL has_correlated_record;
    size_t correlated_record_index;
    COMTRACE_RPC_CORRELATION_TYPE correlation_type;
    BOOL has_endpoint_registration;
    size_t endpoint_record_index;
    BOOL endpoint_match_ambiguous;
    BOOL has_com_operation;
    size_t com_operation_index;
    BOOL com_operation_match_ambiguous;
    size_t pending_next_index;
    size_t pending_previous_index;
    size_t pending_position;
} COMTRACE_RPC_TIMELINE_RECORD;

typedef struct {
    COMTRACE_RPC_TIMELINE_RECORD *records;
    size_t record_count;
    size_t record_capacity;
    size_t *pending_indices;
    size_t pending_count;
    size_t pending_capacity;
    size_t *pending_hash_slots;
    size_t pending_hash_capacity;
    ULONGLONG unmatched_stops;
    ULONGLONG missing_activity_ids;
    ULONGLONG correlated_client_server_calls;
    ULONGLONG ambiguous_client_server_calls;
    ULONGLONG invalid_duration_orders;
    ULONGLONG ambiguous_stops;
} COMTRACE_RPC_TIMELINE;

void comtrace_rpc_timeline_init(COMTRACE_RPC_TIMELINE *timeline);
void comtrace_rpc_timeline_destroy(COMTRACE_RPC_TIMELINE *timeline);

COMTRACE_RPC_TIMELINE_STATUS comtrace_rpc_timeline_add_event(
    COMTRACE_RPC_TIMELINE *timeline,
    const COMTRACE_RPC_TIMELINE_EVENT *event
);

size_t comtrace_rpc_timeline_count(const COMTRACE_RPC_TIMELINE *timeline);
size_t comtrace_rpc_timeline_pending_count(const COMTRACE_RPC_TIMELINE *timeline);

COMTRACE_RPC_TIMELINE_STATUS comtrace_rpc_timeline_correlate_calls(COMTRACE_RPC_TIMELINE *timeline);

COMTRACE_RPC_TIMELINE_STATUS comtrace_rpc_timeline_link_com_operations(
    COMTRACE_RPC_TIMELINE *rpc_timeline,
    const COMTRACE_TIMELINE *com_timeline
);

const COMTRACE_RPC_TIMELINE_RECORD *comtrace_rpc_timeline_get(
    const COMTRACE_RPC_TIMELINE *timeline,
    size_t index
);

#endif
