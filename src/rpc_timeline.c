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
         record->procedure_number != event->procedure_number)) {
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
    COMTRACE_RPC_TIMELINE_RECORD *record;
    COMTRACE_RPC_TIMELINE_MATCH_TYPE match_type;

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
        record_index = timeline->pending_indices[pending_index];
        record = &timeline->records[record_index];
        match_type = get_match_type(record, event);
        if (match_type == COMTRACE_RPC_TIMELINE_MATCH_NONE) {
            continue;
        }

        record->match_type = match_type;
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

        if (pending_index + 1U < timeline->pending_count) {
            memmove(
                &timeline->pending_indices[pending_index],
                &timeline->pending_indices[pending_index + 1U],
                (timeline->pending_count - pending_index - 1U) *
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

void comtrace_rpc_timeline_correlate_calls(COMTRACE_RPC_TIMELINE *timeline)
{
    size_t client_index;

    if (timeline == NULL) {
        return;
    }

    timeline->correlated_client_server_calls = 0ULL;
    timeline->ambiguous_client_server_calls = 0ULL;
    for (client_index = 0U; client_index < timeline->record_count; ++client_index) {
        timeline->records[client_index].has_correlated_record = FALSE;
        timeline->records[client_index].correlated_record_index = 0U;
        timeline->records[client_index].correlation_type = COMTRACE_RPC_CORRELATION_NONE;
    }

    for (client_index = 0U; client_index < timeline->record_count; ++client_index) {
        COMTRACE_RPC_TIMELINE_RECORD *client = &timeline->records[client_index];
        size_t server_index;
        size_t candidate_server = (size_t)-1;
        size_t candidate_count = 0U;

        client->has_correlated_record = FALSE;
        client->correlated_record_index = 0U;
        client->correlation_type = COMTRACE_RPC_CORRELATION_NONE;

        if (client->role != COMTRACE_RPC_ROLE_CLIENT ||
            !client->has_start || !client->has_stop) {
            continue;
        }

        for (server_index = 0U; server_index < timeline->record_count; ++server_index) {
            const COMTRACE_RPC_TIMELINE_RECORD *server = &timeline->records[server_index];

            if (server->role != COMTRACE_RPC_ROLE_SERVER ||
                !server->has_start || !server->has_stop ||
                get_cross_call_match(client, server) == COMTRACE_RPC_CORRELATION_NONE) {
                continue;
            }
            candidate_server = server_index;
            ++candidate_count;
        }

        if (candidate_count > 1U) {
            increment_count(&timeline->ambiguous_client_server_calls);
            continue;
        }
        if (candidate_count != 1U) {
            continue;
        }

        {
            size_t other_client_index;
            size_t reverse_count = 0U;
            COMTRACE_RPC_TIMELINE_RECORD *server =
                &timeline->records[candidate_server];

            for (other_client_index = 0U;
                 other_client_index < timeline->record_count;
                 ++other_client_index) {
                const COMTRACE_RPC_TIMELINE_RECORD *other_client =
                    &timeline->records[other_client_index];

                if (other_client->role == COMTRACE_RPC_ROLE_CLIENT &&
                    other_client->has_start && other_client->has_stop &&
                    get_cross_call_match(other_client, server) !=
                        COMTRACE_RPC_CORRELATION_NONE) {
                    ++reverse_count;
                }
            }

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
    }
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
