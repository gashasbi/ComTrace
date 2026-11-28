#include "rpc_timeline.h"

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static void increment_count(ULONGLONG *value)
{
    if (*value != (ULONGLONG)-1) {
        ++(*value);
    }
}

static int ensure_record_capacity(COMTRACE_RPC_TIMELINE *timeline)
{
    size_t capacity;
    COMTRACE_RPC_TIMELINE_RECORD *records;

    if (timeline->record_count < timeline->record_capacity) {
        return 1;
    }

    capacity = timeline->record_capacity == 0U
        ? 16U
        : timeline->record_capacity * 2U;
    if (capacity < timeline->record_capacity ||
        capacity > (size_t)-1 / sizeof(*records)) {
        return 0;
    }

    records = (COMTRACE_RPC_TIMELINE_RECORD *)realloc(
        timeline->records,
        capacity * sizeof(*records)
    );
    if (records == NULL) {
        return 0;
    }

    timeline->records = records;
    timeline->record_capacity = capacity;
    return 1;
}

static int ensure_pending_capacity(COMTRACE_RPC_TIMELINE *timeline)
{
    size_t capacity;
    size_t *indices;

    if (timeline->pending_count < timeline->pending_capacity) {
        return 1;
    }

    capacity = timeline->pending_capacity == 0U
        ? 16U
        : timeline->pending_capacity * 2U;
    if (capacity < timeline->pending_capacity ||
        capacity > (size_t)-1 / sizeof(*indices)) {
        return 0;
    }

    indices = (size_t *)realloc(
        timeline->pending_indices,
        capacity * sizeof(*indices)
    );
    if (indices == NULL) {
        return 0;
    }

    timeline->pending_indices = indices;
    timeline->pending_capacity = capacity;
    return 1;
}

static int copy_operation(
    wchar_t *destination,
    size_t capacity,
    const wchar_t *source
)
{
    size_t length;

    if (source == NULL || source[0] == L'\0') {
        return 0;
    }

    length = wcslen(source);
    if (length >= capacity) {
        return 0;
    }

    CopyMemory(destination, source, (length + 1U) * sizeof(wchar_t));
    return 1;
}

static int copy_optional_string(
    wchar_t *destination,
    size_t capacity,
    const wchar_t *source
)
{
    size_t length;

    if (source == NULL || source[0] == L'\0') {
        return 1;
    }

    length = wcslen(source);
    if (length >= capacity) {
        return 0;
    }

    CopyMemory(destination, source, (length + 1U) * sizeof(wchar_t));
    return 1;
}

static COMTRACE_RPC_ROLE get_role(const wchar_t *operation)
{
    if (wcscmp(operation, L"RpcClientCall") == 0) {
        return COMTRACE_RPC_ROLE_CLIENT;
    }
    if (wcscmp(operation, L"RpcServerCall") == 0) {
        return COMTRACE_RPC_ROLE_SERVER;
    }
    if (wcscmp(operation, L"FunctionTrace") == 0 ||
        wcscmp(operation, L"FunctionTraceStart16") == 0) {
        return COMTRACE_RPC_ROLE_FUNCTION;
    }
    return COMTRACE_RPC_ROLE_UNKNOWN;
}

static COMTRACE_RPC_LOCATION get_location(const wchar_t *protocol)
{
    if (protocol == NULL) {
        return COMTRACE_RPC_LOCATION_UNKNOWN;
    }
    if (_wcsicmp(protocol, L"ncalrpc") == 0) {
        return COMTRACE_RPC_LOCATION_LOCAL;
    }
    if (_wcsicmp(protocol, L"ncacn_ip_tcp") == 0 ||
        _wcsicmp(protocol, L"ncacn_http") == 0) {
        return COMTRACE_RPC_LOCATION_NETWORK;
    }
    return COMTRACE_RPC_LOCATION_UNKNOWN;
}

static void copy_event_metadata(
    COMTRACE_RPC_TIMELINE_RECORD *record,
    const COMTRACE_RPC_TIMELINE_EVENT *event
)
{
    if (event->activity_ids != NULL) {
        if (event->activity_ids->has_activity_id) {
            record->activity_id = event->activity_ids->activity_id;
            record->has_activity_id = TRUE;
        }
        if (event->activity_ids->has_related_activity_id) {
            record->related_activity_id = event->activity_ids->related_activity_id;
            record->has_related_activity_id = TRUE;
        }
    }

    if (!record->has_interface_uuid && event->has_interface_uuid) {
        record->interface_uuid = event->interface_uuid;
        record->has_interface_uuid = TRUE;
    }
    if (!record->has_procedure_number && event->has_procedure_number) {
        record->procedure_number = event->procedure_number;
        record->has_procedure_number = TRUE;
    }
    if (record->protocol[0] == L'\0' &&
        copy_optional_string(record->protocol, _countof(record->protocol), event->protocol)) {
        record->location = get_location(record->protocol);
    }
    if (record->network_address[0] == L'\0') {
        (void)copy_optional_string(
            record->network_address,
            _countof(record->network_address),
            event->network_address
        );
    }
    if (record->endpoint[0] == L'\0') {
        (void)copy_optional_string(
            record->endpoint,
            _countof(record->endpoint),
            event->endpoint
        );
    }
}

static COMTRACE_RPC_TIMELINE_MATCH_TYPE get_match_type(
    const COMTRACE_RPC_TIMELINE_RECORD *record,
    const COMTRACE_RPC_TIMELINE_EVENT *event
)
{
    BOOL event_has_activity = event->activity_ids != NULL &&
        event->activity_ids->has_activity_id;
    BOOL event_has_related_activity = event->activity_ids != NULL &&
        event->activity_ids->has_related_activity_id;

    if (wcscmp(record->operation, event->operation) != 0 ||
        record->has_activity_id != event_has_activity ||
        (record->has_interface_uuid && event->has_interface_uuid &&
         memcmp(&record->interface_uuid, &event->interface_uuid, sizeof(GUID)) != 0) ||
        (record->has_procedure_number && event->has_procedure_number &&
         record->procedure_number != event->procedure_number) ||
        (record->protocol[0] != L'\0' &&
         event->protocol != NULL && event->protocol[0] != L'\0' &&
         _wcsicmp(record->protocol, event->protocol) != 0) ||
        (record->endpoint[0] != L'\0' &&
         event->endpoint != NULL && event->endpoint[0] != L'\0' &&
         _wcsicmp(record->endpoint, event->endpoint) != 0) ||
        (record->network_address[0] != L'\0' &&
         event->network_address != NULL && event->network_address[0] != L'\0' &&
         _wcsicmp(record->network_address, event->network_address) != 0)) {
        return COMTRACE_RPC_TIMELINE_MATCH_NONE;
    }

    if (record->has_activity_id) {
        if (memcmp(
                &record->activity_id,
                &event->activity_ids->activity_id,
                sizeof(GUID)
            ) == 0) {
            if (record->has_related_activity_id &&
                event_has_related_activity &&
                memcmp(
                    &record->related_activity_id,
                    &event->activity_ids->related_activity_id,
                    sizeof(GUID)
                ) != 0) {
                return COMTRACE_RPC_TIMELINE_MATCH_NONE;
            }
            return COMTRACE_RPC_TIMELINE_MATCH_ACTIVITY_ID;
        }
        return COMTRACE_RPC_TIMELINE_MATCH_NONE;
    }

    if (record->has_related_activity_id || event_has_related_activity) {
        if (record->has_related_activity_id &&
            event_has_related_activity &&
            memcmp(
                &record->related_activity_id,
                &event->activity_ids->related_activity_id,
                sizeof(GUID)
            ) == 0) {
            return COMTRACE_RPC_TIMELINE_MATCH_RELATED_ACTIVITY_ID;
        }
        return COMTRACE_RPC_TIMELINE_MATCH_NONE;
    }

    if (record->process_id == event->process_id &&
        record->thread_id == event->thread_id) {
        return COMTRACE_RPC_TIMELINE_MATCH_PROCESS_THREAD;
    }

    return COMTRACE_RPC_TIMELINE_MATCH_NONE;
}

void comtrace_rpc_timeline_init(COMTRACE_RPC_TIMELINE *timeline)
{
    if (timeline != NULL) {
        ZeroMemory(timeline, sizeof(*timeline));
    }
}

void comtrace_rpc_timeline_destroy(COMTRACE_RPC_TIMELINE *timeline)
{
    if (timeline == NULL) {
        return;
    }

    free(timeline->records);
    free(timeline->pending_indices);
    ZeroMemory(timeline, sizeof(*timeline));
}

COMTRACE_RPC_TIMELINE_STATUS comtrace_rpc_timeline_add_event(
    COMTRACE_RPC_TIMELINE *timeline,
    const COMTRACE_RPC_TIMELINE_EVENT *event
)
{
    size_t position;
    size_t record_index;
    size_t selected_pending_index = (size_t)-1;
    size_t cross_candidate_index = (size_t)-1;
    size_t cross_candidate_count = 0U;
    size_t same_thread_candidate_count = 0U;
    BOOL ambiguous_match = FALSE;
    COMTRACE_RPC_TIMELINE_RECORD *record;
    COMTRACE_RPC_TIMELINE_MATCH_TYPE match_type;
    COMTRACE_RPC_TIMELINE_MATCH_TYPE selected_match_type = COMTRACE_RPC_TIMELINE_MATCH_NONE;

    if (timeline == NULL ||
        event == NULL ||
        event->operation == NULL ||
        event->operation[0] == L'\0' ||
        (event->phase != COMTRACE_RPC_TIMELINE_PHASE_START &&
         event->phase != COMTRACE_RPC_TIMELINE_PHASE_STOP)) {
        return COMTRACE_RPC_TIMELINE_INVALID_ARGUMENT;
    }

    if (event->phase == COMTRACE_RPC_TIMELINE_PHASE_START) {
        if (!ensure_record_capacity(timeline) ||
            !ensure_pending_capacity(timeline)) {
            return COMTRACE_RPC_TIMELINE_ALLOCATION_FAILED;
        }

        record_index = timeline->record_count;
        record = &timeline->records[record_index];
        ZeroMemory(record, sizeof(*record));
        if (!copy_operation(
                record->operation,
                _countof(record->operation),
                event->operation)) {
            return COMTRACE_RPC_TIMELINE_INVALID_ARGUMENT;
        }

        record->process_id = event->process_id;
        record->thread_id = event->thread_id;
        record->role = get_role(event->operation);
        record->start_timestamp = event->timestamp;
        record->has_start = TRUE;
        copy_event_metadata(record, event);
        ++timeline->record_count;

        timeline->pending_indices[timeline->pending_count] = record_index;
        ++timeline->pending_count;
        if (event->activity_ids == NULL ||
            !event->activity_ids->has_activity_id) {
            increment_count(&timeline->missing_activity_ids);
        }
        return COMTRACE_RPC_TIMELINE_SUCCESS;
    }

    for (position = timeline->pending_count; position > 0U; --position) {
        size_t pending_index = position - 1U;
        size_t candidate_record_index = timeline->pending_indices[pending_index];
        const COMTRACE_RPC_TIMELINE_RECORD *candidate =
            &timeline->records[candidate_record_index];

        match_type = get_match_type(candidate, event);
        if (match_type == COMTRACE_RPC_TIMELINE_MATCH_NONE) {
            continue;
        }

        if (match_type == COMTRACE_RPC_TIMELINE_MATCH_PROCESS_THREAD) {
            selected_pending_index = pending_index;
            selected_match_type = match_type;
            break;
        }

        if (candidate->process_id == event->process_id &&
            candidate->thread_id == event->thread_id) {
            if (same_thread_candidate_count == 0U) {
                selected_pending_index = pending_index;
                selected_match_type = match_type;
            }
            ++same_thread_candidate_count;
        } else {
            if (cross_candidate_count == 0U) {
                cross_candidate_index = pending_index;
            }
            ++cross_candidate_count;
        }
    }

    if (same_thread_candidate_count == 0U &&
        selected_pending_index == (size_t)-1) {
        if (cross_candidate_count == 1U) {
            selected_pending_index = cross_candidate_index;
            record_index = timeline->pending_indices[selected_pending_index];
            selected_match_type = get_match_type(&timeline->records[record_index], event);
        } else if (cross_candidate_count > 1U) {
            ambiguous_match = TRUE;
        }
    }

    if (selected_pending_index != (size_t)-1) {
        record_index = timeline->pending_indices[selected_pending_index];
        record = &timeline->records[record_index];
        record->match_type = selected_match_type;
        record->has_stop = TRUE;
        record->stop_process_id = event->process_id;
        record->stop_thread_id = event->thread_id;
        record->stop_timestamp = event->timestamp;
        record->has_status = event->has_status;
        record->status = event->status;
        record->status_is_failure = event->has_status && event->status != 0U;
        if (event->timestamp >= record->start_timestamp) {
            record->duration_ticks = event->timestamp - record->start_timestamp;
            record->has_duration = TRUE;
        } else {
            record->invalid_duration_order = TRUE;
            increment_count(&timeline->invalid_duration_orders);
        }
        if (!record->has_related_activity_id &&
            event->activity_ids != NULL &&
            event->activity_ids->has_related_activity_id) {
            record->related_activity_id =
                event->activity_ids->related_activity_id;
            record->has_related_activity_id = TRUE;
        }
        if (!record->has_interface_uuid ||
            !record->has_procedure_number ||
            record->protocol[0] == L'\0' ||
            record->endpoint[0] == L'\0') {
            copy_event_metadata(record, event);
        }

        if (selected_pending_index + 1U < timeline->pending_count) {
            memmove(
                &timeline->pending_indices[selected_pending_index],
                &timeline->pending_indices[selected_pending_index + 1U],
                (timeline->pending_count - selected_pending_index - 1U) *
                    sizeof(timeline->pending_indices[0])
            );
        }
        --timeline->pending_count;
        if (event->activity_ids == NULL ||
            !event->activity_ids->has_activity_id) {
            increment_count(&timeline->missing_activity_ids);
        }
        return COMTRACE_RPC_TIMELINE_SUCCESS;
    }

    if (!ensure_record_capacity(timeline)) {
        return COMTRACE_RPC_TIMELINE_ALLOCATION_FAILED;
    }

    record_index = timeline->record_count;
    record = &timeline->records[record_index];
    ZeroMemory(record, sizeof(*record));
    if (!copy_operation(
            record->operation,
            _countof(record->operation),
            event->operation)) {
        return COMTRACE_RPC_TIMELINE_INVALID_ARGUMENT;
    }

    record->process_id = event->process_id;
    record->thread_id = event->thread_id;
    record->role = get_role(event->operation);
    record->stop_process_id = event->process_id;
    record->stop_thread_id = event->thread_id;
    record->stop_timestamp = event->timestamp;
    record->has_stop = TRUE;
    record->ambiguous_match = ambiguous_match;
    if (ambiguous_match) {
        increment_count(&timeline->ambiguous_stops);
    }
    record->has_status = event->has_status;
    record->status = event->status;
    record->status_is_failure = event->has_status && event->status != 0U;
    copy_event_metadata(record, event);
    ++timeline->record_count;
    increment_count(&timeline->unmatched_stops);
    if (event->activity_ids == NULL ||
        !event->activity_ids->has_activity_id) {
        increment_count(&timeline->missing_activity_ids);
    }

    return COMTRACE_RPC_TIMELINE_SUCCESS;
}

static COMTRACE_RPC_CORRELATION_TYPE get_cross_call_match(
    const COMTRACE_RPC_TIMELINE_RECORD *left,
    const COMTRACE_RPC_TIMELINE_RECORD *right
)
{
    if (!left->has_activity_id || !right->has_activity_id ||
        !left->has_interface_uuid || !right->has_interface_uuid ||
        !left->has_procedure_number || !right->has_procedure_number ||
        memcmp(&left->interface_uuid, &right->interface_uuid, sizeof(GUID)) != 0 ||
        left->procedure_number != right->procedure_number) {
        return COMTRACE_RPC_CORRELATION_NONE;
    }

    if (memcmp(&left->activity_id, &right->activity_id, sizeof(GUID)) == 0) {
        return COMTRACE_RPC_CORRELATION_ACTIVITY_ID;
    }

    if ((left->has_related_activity_id &&
         memcmp(&left->related_activity_id, &right->activity_id, sizeof(GUID)) == 0) ||
        (right->has_related_activity_id &&
         memcmp(&right->related_activity_id, &left->activity_id, sizeof(GUID)) == 0)) {
        return COMTRACE_RPC_CORRELATION_RELATED_ACTIVITY_ID;
    }

    return COMTRACE_RPC_CORRELATION_NONE;
}

typedef struct {
    GUID interface_uuid;
    ULONG procedure_number;
    GUID activity_id;
    COMTRACE_RPC_ROLE role;
    size_t record_index;
} COMTRACE_RPC_CALL_LOOKUP_KEY;

static int compare_rpc_call_lookup_group(
    const COMTRACE_RPC_CALL_LOOKUP_KEY *left,
    const COMTRACE_RPC_CALL_LOOKUP_KEY *right
)
{
    int result = memcmp(
        &left->interface_uuid,
        &right->interface_uuid,
        sizeof(left->interface_uuid)
    );

    if (result != 0) {
        return result;
    }
    if (left->procedure_number < right->procedure_number) {
        return -1;
    }
    if (left->procedure_number > right->procedure_number) {
        return 1;
    }

    result = memcmp(
        &left->activity_id,
        &right->activity_id,
        sizeof(left->activity_id)
    );
    if (result != 0) {
        return result;
    }
    if (left->role < right->role) {
        return -1;
    }
    if (left->role > right->role) {
        return 1;
    }

    return 0;
}

static int compare_rpc_call_lookup_keys(
    const void *left_value,
    const void *right_value
)
{
    const COMTRACE_RPC_CALL_LOOKUP_KEY *left =
        (const COMTRACE_RPC_CALL_LOOKUP_KEY *)left_value;
    const COMTRACE_RPC_CALL_LOOKUP_KEY *right =
        (const COMTRACE_RPC_CALL_LOOKUP_KEY *)right_value;
    int result = compare_rpc_call_lookup_group(left, right);

    if (result != 0) {
        return result;
    }
    if (left->record_index < right->record_index) {
        return -1;
    }
    if (left->record_index > right->record_index) {
        return 1;
    }

    return 0;
}

static void find_rpc_call_lookup_range(
    const COMTRACE_RPC_CALL_LOOKUP_KEY *keys,
    size_t key_count,
    const GUID *interface_uuid,
    ULONG procedure_number,
    const GUID *activity_id,
    COMTRACE_RPC_ROLE role,
    size_t *range_start,
    size_t *range_end
)
{
    COMTRACE_RPC_CALL_LOOKUP_KEY search_key;
    size_t low = 0U;
    size_t high = key_count;

    ZeroMemory(&search_key, sizeof(search_key));
    search_key.interface_uuid = *interface_uuid;
    search_key.procedure_number = procedure_number;
    search_key.activity_id = *activity_id;
    search_key.role = role;

    while (low < high) {
        size_t middle = low + (high - low) / 2U;

        if (compare_rpc_call_lookup_group(&keys[middle], &search_key) < 0) {
            low = middle + 1U;
        } else {
            high = middle;
        }
    }
    *range_start = low;

    high = key_count;
    while (low < high) {
        size_t middle = low + (high - low) / 2U;

        if (compare_rpc_call_lookup_group(&keys[middle], &search_key) <= 0) {
            low = middle + 1U;
        } else {
            high = middle;
        }
    }
    *range_end = low;
}

static void add_rpc_call_candidates_from_index(
    const COMTRACE_RPC_CALL_LOOKUP_KEY *keys,
    size_t key_count,
    const COMTRACE_RPC_TIMELINE *timeline,
    const COMTRACE_RPC_TIMELINE_RECORD *record,
    const GUID *activity_id,
    COMTRACE_RPC_ROLE target_role,
    size_t *candidate_marks,
    size_t mark_value,
    size_t *candidate_count,
    size_t *candidate_index
)
{
    size_t range_start;
    size_t range_end;
    size_t position;

    if (*candidate_count > 1U) {
        return;
    }

    find_rpc_call_lookup_range(
        keys,
        key_count,
        &record->interface_uuid,
        record->procedure_number,
        activity_id,
        target_role,
        &range_start,
        &range_end
    );

    for (position = range_start; position < range_end; ++position) {
        size_t record_index = keys[position].record_index;

        if (candidate_marks[record_index] == mark_value) {
            continue;
        }
        candidate_marks[record_index] = mark_value;

        if (get_cross_call_match(
                record,
                &timeline->records[record_index]
            ) == COMTRACE_RPC_CORRELATION_NONE) {
            continue;
        }

        if (*candidate_count == 0U) {
            *candidate_index = record_index;
        }
        ++(*candidate_count);
        if (*candidate_count > 1U) {
            return;
        }
    }
}

static size_t count_rpc_call_candidates(
    const COMTRACE_RPC_CALL_LOOKUP_KEY *primary_keys,
    size_t primary_key_count,
    const COMTRACE_RPC_CALL_LOOKUP_KEY *related_keys,
    size_t related_key_count,
    const COMTRACE_RPC_TIMELINE *timeline,
    const COMTRACE_RPC_TIMELINE_RECORD *record,
    COMTRACE_RPC_ROLE target_role,
    size_t *candidate_marks,
    size_t mark_value,
    size_t *candidate_index
)
{
    size_t candidate_count = 0U;

    add_rpc_call_candidates_from_index(
        primary_keys,
        primary_key_count,
        timeline,
        record,
        &record->activity_id,
        target_role,
        candidate_marks,
        mark_value,
        &candidate_count,
        candidate_index
    );

    if (record->has_related_activity_id) {
        add_rpc_call_candidates_from_index(
            primary_keys,
            primary_key_count,
            timeline,
            record,
            &record->related_activity_id,
            target_role,
            candidate_marks,
            mark_value,
            &candidate_count,
            candidate_index
        );
    }

    add_rpc_call_candidates_from_index(
        related_keys,
        related_key_count,
        timeline,
        record,
        &record->activity_id,
        target_role,
        candidate_marks,
        mark_value,
        &candidate_count,
        candidate_index
    );

    return candidate_count;
}

void comtrace_rpc_timeline_correlate_calls(COMTRACE_RPC_TIMELINE *timeline)
{
    COMTRACE_RPC_CALL_LOOKUP_KEY *primary_keys = NULL;
    COMTRACE_RPC_CALL_LOOKUP_KEY *related_keys = NULL;
    size_t *candidate_marks = NULL;
    size_t primary_key_count = 0U;
    size_t related_key_count = 0U;
    size_t primary_position = 0U;
    size_t related_position = 0U;
    size_t client_index;
    size_t record_index;
    size_t mark_value = 0U;

    if (timeline == NULL) {
        return;
    }

    timeline->correlated_client_server_calls = 0ULL;
    timeline->ambiguous_client_server_calls = 0ULL;
    for (record_index = 0U; record_index < timeline->record_count; ++record_index) {
        timeline->records[record_index].has_correlated_record = FALSE;
        timeline->records[record_index].correlated_record_index = 0U;
        timeline->records[record_index].correlation_type = COMTRACE_RPC_CORRELATION_NONE;

        if (!timeline->records[record_index].has_start ||
            !timeline->records[record_index].has_stop ||
            !timeline->records[record_index].has_activity_id ||
            !timeline->records[record_index].has_interface_uuid ||
            !timeline->records[record_index].has_procedure_number ||
            (timeline->records[record_index].role != COMTRACE_RPC_ROLE_CLIENT &&
             timeline->records[record_index].role != COMTRACE_RPC_ROLE_SERVER)) {
            continue;
        }

        ++primary_key_count;
        if (timeline->records[record_index].has_related_activity_id) {
            ++related_key_count;
        }
    }

    if (primary_key_count == 0U) {
        return;
    }

    if (primary_key_count > (size_t)-1 / sizeof(*primary_keys) ||
        related_key_count > (size_t)-1 / sizeof(*related_keys) ||
        timeline->record_count > (size_t)-1 / sizeof(*candidate_marks)) {
        return;
    }

    primary_keys = (COMTRACE_RPC_CALL_LOOKUP_KEY *)malloc(
        primary_key_count * sizeof(*primary_keys)
    );
    if (related_key_count != 0U) {
        related_keys = (COMTRACE_RPC_CALL_LOOKUP_KEY *)malloc(
            related_key_count * sizeof(*related_keys)
        );
    }
    candidate_marks = (size_t *)calloc(
        timeline->record_count,
        sizeof(*candidate_marks)
    );

    if (primary_keys == NULL ||
        (related_key_count != 0U && related_keys == NULL) ||
        candidate_marks == NULL) {
        free(primary_keys);
        free(related_keys);
        free(candidate_marks);
        return;
    }

    for (record_index = 0U; record_index < timeline->record_count; ++record_index) {
        const COMTRACE_RPC_TIMELINE_RECORD *record =
            &timeline->records[record_index];
        COMTRACE_RPC_CALL_LOOKUP_KEY *key;

        if (!record->has_start ||
            !record->has_stop ||
            !record->has_activity_id ||
            !record->has_interface_uuid ||
            !record->has_procedure_number ||
            (record->role != COMTRACE_RPC_ROLE_CLIENT &&
             record->role != COMTRACE_RPC_ROLE_SERVER)) {
            continue;
        }

        key = &primary_keys[primary_position];
        key->interface_uuid = record->interface_uuid;
        key->procedure_number = record->procedure_number;
        key->activity_id = record->activity_id;
        key->role = record->role;
        key->record_index = record_index;
        ++primary_position;

        if (record->has_related_activity_id) {
            key = &related_keys[related_position];
            key->interface_uuid = record->interface_uuid;
            key->procedure_number = record->procedure_number;
            key->activity_id = record->related_activity_id;
            key->role = record->role;
            key->record_index = record_index;
            ++related_position;
        }
    }

    qsort(
        primary_keys,
        primary_key_count,
        sizeof(*primary_keys),
        compare_rpc_call_lookup_keys
    );
    if (related_key_count != 0U) {
        qsort(
            related_keys,
            related_key_count,
            sizeof(*related_keys),
            compare_rpc_call_lookup_keys
        );
    }

    for (client_index = 0U; client_index < timeline->record_count; ++client_index) {
        COMTRACE_RPC_TIMELINE_RECORD *client = &timeline->records[client_index];
        size_t candidate_server = 0U;
        size_t candidate_count;
        size_t candidate_client = 0U;
        size_t reverse_count;
        COMTRACE_RPC_TIMELINE_RECORD *server;

        if (client->role != COMTRACE_RPC_ROLE_CLIENT ||
            !client->has_start ||
            !client->has_stop ||
            !client->has_activity_id ||
            !client->has_interface_uuid ||
            !client->has_procedure_number) {
            continue;
        }

        ++mark_value;
        candidate_count = count_rpc_call_candidates(
            primary_keys,
            primary_key_count,
            related_keys,
            related_key_count,
            timeline,
            client,
            COMTRACE_RPC_ROLE_SERVER,
            candidate_marks,
            mark_value,
            &candidate_server
        );

        if (candidate_count > 1U) {
            increment_count(&timeline->ambiguous_client_server_calls);
            continue;
        }
        if (candidate_count != 1U) {
            continue;
        }

        server = &timeline->records[candidate_server];
        ++mark_value;
        reverse_count = count_rpc_call_candidates(
            primary_keys,
            primary_key_count,
            related_keys,
            related_key_count,
            timeline,
            server,
            COMTRACE_RPC_ROLE_CLIENT,
            candidate_marks,
            mark_value,
            &candidate_client
        );

        if (reverse_count != 1U || server->has_correlated_record) {
            increment_count(&timeline->ambiguous_client_server_calls);
            continue;
        }

        client->has_correlated_record = TRUE;
        client->correlated_record_index = candidate_server;
        client->correlation_type = get_cross_call_match(client, server);
        server->has_correlated_record = TRUE;
        server->correlated_record_index = client_index;
        server->correlation_type = client->correlation_type;
        increment_count(&timeline->correlated_client_server_calls);
    }

    free(primary_keys);
    free(related_keys);
    free(candidate_marks);
}

typedef struct {
    GUID activity_id;
    GUID interface_uuid;
    size_t record_index;
} COMTRACE_RPC_COM_LINK_KEY;

static int compare_rpc_com_link_keys(
    const void *left_value,
    const void *right_value
)
{
    const COMTRACE_RPC_COM_LINK_KEY *left =
        (const COMTRACE_RPC_COM_LINK_KEY *)left_value;
    const COMTRACE_RPC_COM_LINK_KEY *right =
        (const COMTRACE_RPC_COM_LINK_KEY *)right_value;
    int result = memcmp(
        &left->activity_id,
        &right->activity_id,
        sizeof(left->activity_id)
    );

    if (result != 0) {
        return result;
    }

    return memcmp(
        &left->interface_uuid,
        &right->interface_uuid,
        sizeof(left->interface_uuid)
    );
}

COMTRACE_RPC_TIMELINE_STATUS comtrace_rpc_timeline_link_com_operations(
    COMTRACE_RPC_TIMELINE *rpc_timeline,
    const COMTRACE_TIMELINE *com_timeline
)
{
    COMTRACE_RPC_COM_LINK_KEY *keys;
    size_t key_count = 0U;
    size_t key_position = 0U;
    size_t rpc_index;
    size_t com_index;

    if (rpc_timeline == NULL || com_timeline == NULL) {
        return COMTRACE_RPC_TIMELINE_INVALID_ARGUMENT;
    }

    for (rpc_index = 0U; rpc_index < rpc_timeline->record_count; ++rpc_index) {
        COMTRACE_RPC_TIMELINE_RECORD *record = &rpc_timeline->records[rpc_index];
        record->has_com_operation = FALSE;
        record->com_operation_index = 0U;
        record->com_operation_match_ambiguous = FALSE;
    }

    for (com_index = 0U; com_index < com_timeline->record_count; ++com_index) {
        const COMTRACE_TIMELINE_RECORD *record = &com_timeline->records[com_index];

        if (record->has_start &&
            record->has_activity_ids &&
            record->activity_ids.has_activity_id &&
            record->has_object_identity &&
            record->object_identity.has_iid) {
            ++key_count;
        }
    }

    if (key_count == 0U) {
        return COMTRACE_RPC_TIMELINE_SUCCESS;
    }

    if (key_count > (size_t)-1 / sizeof(*keys)) {
        return COMTRACE_RPC_TIMELINE_ALLOCATION_FAILED;
    }

    keys = (COMTRACE_RPC_COM_LINK_KEY *)malloc(key_count * sizeof(*keys));
    if (keys == NULL) {
        return COMTRACE_RPC_TIMELINE_ALLOCATION_FAILED;
    }

    for (com_index = 0U; com_index < com_timeline->record_count; ++com_index) {
        const COMTRACE_TIMELINE_RECORD *record = &com_timeline->records[com_index];

        if (!record->has_start ||
            !record->has_activity_ids ||
            !record->activity_ids.has_activity_id ||
            !record->has_object_identity ||
            !record->object_identity.has_iid) {
            continue;
        }

        keys[key_position].activity_id = record->activity_ids.activity_id;
        keys[key_position].interface_uuid = record->object_identity.iid;
        keys[key_position].record_index = com_index;
        ++key_position;
    }

    qsort(keys, key_count, sizeof(*keys), compare_rpc_com_link_keys);

    for (rpc_index = 0U; rpc_index < rpc_timeline->record_count; ++rpc_index) {
        COMTRACE_RPC_TIMELINE_RECORD *rpc_record =
            &rpc_timeline->records[rpc_index];
        COMTRACE_RPC_COM_LINK_KEY search_key;
        size_t low = 0U;
        size_t high = key_count;

        if (!rpc_record->has_start ||
            !rpc_record->has_activity_id ||
            !rpc_record->has_interface_uuid) {
            continue;
        }

        search_key.activity_id = rpc_record->activity_id;
        search_key.interface_uuid = rpc_record->interface_uuid;
        search_key.record_index = 0U;

        while (low < high) {
            size_t middle = low + (high - low) / 2U;

            if (compare_rpc_com_link_keys(&keys[middle], &search_key) < 0) {
                low = middle + 1U;
            } else {
                high = middle;
            }
        }

        if (low == key_count ||
            compare_rpc_com_link_keys(&keys[low], &search_key) != 0) {
            continue;
        }

        if (low + 1U < key_count &&
            compare_rpc_com_link_keys(&keys[low], &keys[low + 1U]) == 0) {
            rpc_record->com_operation_match_ambiguous = TRUE;
        } else {
            rpc_record->has_com_operation = TRUE;
            rpc_record->com_operation_index = keys[low].record_index;
        }
    }

    free(keys);
    return COMTRACE_RPC_TIMELINE_SUCCESS;
}

size_t comtrace_rpc_timeline_count(const COMTRACE_RPC_TIMELINE *timeline)
{
    return timeline == NULL ? 0U : timeline->record_count;
}

size_t comtrace_rpc_timeline_pending_count(const COMTRACE_RPC_TIMELINE *timeline)
{
    return timeline == NULL ? 0U : timeline->pending_count;
}

const COMTRACE_RPC_TIMELINE_RECORD *comtrace_rpc_timeline_get(
    const COMTRACE_RPC_TIMELINE *timeline,
    size_t index
)
{
    if (timeline == NULL || index >= timeline->record_count) {
        return NULL;
    }

    return &timeline->records[index];
}
