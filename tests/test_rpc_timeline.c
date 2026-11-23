#include "rpc_timeline.h"

#include <stdio.h>
#include <string.h>
#include <wchar.h>

static const GUID activity_id = {
    0x12345678, 0x9ABC, 0x4DEF,
    {0x80, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07}
};

static const GUID interface_id = {
    0x87654321, 0xCBA9, 0x4FED,
    {0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F}
};

static int test_activity_pair(void)
{
    COMTRACE_RPC_TIMELINE timeline;
    COMTRACE_ACTIVITY_IDS ids;
    COMTRACE_RPC_TIMELINE_EVENT event;
    const COMTRACE_RPC_TIMELINE_RECORD *record;

    comtrace_rpc_timeline_init(&timeline);
    ZeroMemory(&ids, sizeof(ids));
    ids.activity_id = activity_id;
    ids.has_activity_id = TRUE;

    ZeroMemory(&event, sizeof(event));
    event.operation = L"RpcClientCall";
    event.phase = COMTRACE_RPC_TIMELINE_PHASE_START;
    event.process_id = 100U;
    event.thread_id = 200U;
    event.timestamp = 1000ULL;
    event.activity_ids = &ids;
    event.has_interface_uuid = TRUE;
    event.interface_uuid = interface_id;
    event.has_procedure_number = TRUE;
    event.procedure_number = 7U;
    if (comtrace_rpc_timeline_add_event(&timeline, &event) !=
        COMTRACE_RPC_TIMELINE_SUCCESS) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    event.phase = COMTRACE_RPC_TIMELINE_PHASE_STOP;
    event.timestamp = 1400ULL;
    event.has_interface_uuid = FALSE;
    event.has_procedure_number = FALSE;
    if (comtrace_rpc_timeline_add_event(&timeline, &event) !=
        COMTRACE_RPC_TIMELINE_SUCCESS) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    record = comtrace_rpc_timeline_get(&timeline, 0U);
    if (record == NULL ||
        !record->has_start ||
        !record->has_stop ||
        !record->has_duration ||
        record->duration_ticks != 400ULL ||
        record->match_type != COMTRACE_RPC_TIMELINE_MATCH_ACTIVITY_ID ||
        !record->has_interface_uuid ||
        memcmp(&record->interface_uuid, &interface_id, sizeof(GUID)) != 0 ||
        !record->has_procedure_number ||
        record->procedure_number != 7U ||
        timeline.pending_count != 0U) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    comtrace_rpc_timeline_destroy(&timeline);
    return 1;
}

static int test_fallback_and_one_sided_identifiers(void)
{
    COMTRACE_RPC_TIMELINE timeline;
    COMTRACE_ACTIVITY_IDS ids;
    COMTRACE_RPC_TIMELINE_EVENT event;
    const COMTRACE_RPC_TIMELINE_RECORD *record;

    comtrace_rpc_timeline_init(&timeline);
    ZeroMemory(&event, sizeof(event));
    event.operation = L"RpcServerCall";
    event.phase = COMTRACE_RPC_TIMELINE_PHASE_START;
    event.process_id = 10U;
    event.thread_id = 20U;
    event.timestamp = 10ULL;
    if (comtrace_rpc_timeline_add_event(&timeline, &event) !=
        COMTRACE_RPC_TIMELINE_SUCCESS) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    event.phase = COMTRACE_RPC_TIMELINE_PHASE_STOP;
    event.timestamp = 15ULL;
    if (comtrace_rpc_timeline_add_event(&timeline, &event) !=
        COMTRACE_RPC_TIMELINE_SUCCESS) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    record = comtrace_rpc_timeline_get(&timeline, 0U);
    if (record == NULL ||
        record->match_type != COMTRACE_RPC_TIMELINE_MATCH_PROCESS_THREAD ||
        !record->has_duration ||
        record->duration_ticks != 5ULL) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    ZeroMemory(&ids, sizeof(ids));
    ids.activity_id = activity_id;
    ids.has_activity_id = TRUE;
    event.phase = COMTRACE_RPC_TIMELINE_PHASE_START;
    event.timestamp = 20ULL;
    event.activity_ids = &ids;
    if (comtrace_rpc_timeline_add_event(&timeline, &event) !=
        COMTRACE_RPC_TIMELINE_SUCCESS) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    event.phase = COMTRACE_RPC_TIMELINE_PHASE_STOP;
    event.timestamp = 25ULL;
    event.activity_ids = NULL;
    if (comtrace_rpc_timeline_add_event(&timeline, &event) !=
        COMTRACE_RPC_TIMELINE_SUCCESS) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    if (timeline.pending_count != 1U ||
        timeline.unmatched_stops != 1ULL ||
        comtrace_rpc_timeline_count(&timeline) != 3U) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    record = comtrace_rpc_timeline_get(&timeline, 1U);
    if (record == NULL || record->has_stop) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    comtrace_rpc_timeline_destroy(&timeline);
    return 1;
}

static int test_activity_id_pair_across_process_and_thread(void)
{
    COMTRACE_RPC_TIMELINE timeline;
    COMTRACE_ACTIVITY_IDS ids;
    COMTRACE_RPC_TIMELINE_EVENT event;
    const COMTRACE_RPC_TIMELINE_RECORD *record;

    comtrace_rpc_timeline_init(&timeline);
    ZeroMemory(&ids, sizeof(ids));
    ids.activity_id = activity_id;
    ids.has_activity_id = TRUE;

    ZeroMemory(&event, sizeof(event));
    event.operation = L"RpcClientCall";
    event.phase = COMTRACE_RPC_TIMELINE_PHASE_START;
    event.process_id = 100U;
    event.thread_id = 200U;
    event.timestamp = 1000ULL;
    event.activity_ids = &ids;
    event.has_interface_uuid = TRUE;
    event.interface_uuid = interface_id;
    event.has_procedure_number = TRUE;
    event.procedure_number = 7U;

    if (comtrace_rpc_timeline_add_event(&timeline, &event) !=
        COMTRACE_RPC_TIMELINE_SUCCESS) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    event.phase = COMTRACE_RPC_TIMELINE_PHASE_STOP;
    event.process_id = 101U;
    event.thread_id = 201U;
    event.timestamp = 1500ULL;

    if (comtrace_rpc_timeline_add_event(&timeline, &event) !=
        COMTRACE_RPC_TIMELINE_SUCCESS) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    record = comtrace_rpc_timeline_get(&timeline, 0U);
    if (record == NULL ||
        !record->has_start ||
        !record->has_stop ||
        !record->has_duration ||
        record->duration_ticks != 500ULL ||
        record->match_type != COMTRACE_RPC_TIMELINE_MATCH_ACTIVITY_ID ||
        record->process_id != 100U ||
        record->thread_id != 200U ||
        record->stop_process_id != 101U ||
        record->stop_thread_id != 201U ||
        comtrace_rpc_timeline_pending_count(&timeline) != 0U) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    comtrace_rpc_timeline_destroy(&timeline);
    return 1;
}

static int test_cross_process_client_server_correlation(void)
{
    COMTRACE_RPC_TIMELINE timeline;
    COMTRACE_ACTIVITY_IDS ids;
    COMTRACE_RPC_TIMELINE_EVENT event;
    const COMTRACE_RPC_TIMELINE_RECORD *client;
    const COMTRACE_RPC_TIMELINE_RECORD *server;

    comtrace_rpc_timeline_init(&timeline);
    ZeroMemory(&ids, sizeof(ids));
    ids.activity_id = activity_id;
    ids.has_activity_id = TRUE;
    ZeroMemory(&event, sizeof(event));
    event.operation = L"RpcClientCall";
    event.phase = COMTRACE_RPC_TIMELINE_PHASE_START;
    event.process_id = 100U;
    event.thread_id = 10U;
    event.timestamp = 100ULL;
    event.activity_ids = &ids;
    event.has_interface_uuid = TRUE;
    event.interface_uuid = interface_id;
    event.has_procedure_number = TRUE;
    event.procedure_number = 4U;
    event.protocol = L"ncalrpc";
    if (comtrace_rpc_timeline_add_event(&timeline, &event) !=
        COMTRACE_RPC_TIMELINE_SUCCESS) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    event.operation = L"RpcServerCall";
    event.process_id = 200U;
    event.thread_id = 20U;
    event.timestamp = 110ULL;
    event.protocol = L"ncacn_ip_tcp";
    event.network_address = L"192.0.2.10";
    event.endpoint = L"49664";
    if (comtrace_rpc_timeline_add_event(&timeline, &event) !=
        COMTRACE_RPC_TIMELINE_SUCCESS) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    event.phase = COMTRACE_RPC_TIMELINE_PHASE_STOP;
    event.timestamp = 130ULL;
    event.protocol = NULL;
    event.network_address = NULL;
    event.endpoint = NULL;
    if (comtrace_rpc_timeline_add_event(&timeline, &event) !=
        COMTRACE_RPC_TIMELINE_SUCCESS) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    event.operation = L"RpcClientCall";
    event.process_id = 100U;
    event.thread_id = 10U;
    event.timestamp = 140ULL;
    if (comtrace_rpc_timeline_add_event(&timeline, &event) !=
        COMTRACE_RPC_TIMELINE_SUCCESS) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    comtrace_rpc_timeline_correlate_calls(&timeline);
    client = comtrace_rpc_timeline_get(&timeline, 0U);
    server = comtrace_rpc_timeline_get(&timeline, 1U);
    if (client == NULL || server == NULL ||
        client->role != COMTRACE_RPC_ROLE_CLIENT ||
        server->role != COMTRACE_RPC_ROLE_SERVER ||
        client->location != COMTRACE_RPC_LOCATION_LOCAL ||
        server->location != COMTRACE_RPC_LOCATION_NETWORK ||
        wcscmp(server->network_address, L"192.0.2.10") != 0 ||
        wcscmp(server->endpoint, L"49664") != 0 ||
        !client->has_correlated_record ||
        !server->has_correlated_record ||
        client->correlated_record_index != 1U ||
        server->correlated_record_index != 0U ||
        client->correlation_type != COMTRACE_RPC_CORRELATION_ACTIVITY_ID ||
        timeline.correlated_client_server_calls != 1ULL ||
        timeline.ambiguous_client_server_calls != 0ULL) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    comtrace_rpc_timeline_destroy(&timeline);
    return 1;
}

static int test_ambiguous_client_server_correlation_is_not_guessed(void)
{
    COMTRACE_RPC_TIMELINE timeline;
    COMTRACE_ACTIVITY_IDS ids;
    COMTRACE_RPC_TIMELINE_EVENT event;
    size_t index;

    comtrace_rpc_timeline_init(&timeline);
    ZeroMemory(&ids, sizeof(ids));
    ids.activity_id = activity_id;
    ids.has_activity_id = TRUE;
    ZeroMemory(&event, sizeof(event));
    event.operation = L"RpcClientCall";
    event.phase = COMTRACE_RPC_TIMELINE_PHASE_START;
    event.process_id = 10U;
    event.thread_id = 1U;
    event.timestamp = 10ULL;
    event.activity_ids = &ids;
    event.has_interface_uuid = TRUE;
    event.interface_uuid = interface_id;
    event.has_procedure_number = TRUE;
    event.procedure_number = 3U;
    if (comtrace_rpc_timeline_add_event(&timeline, &event) !=
        COMTRACE_RPC_TIMELINE_SUCCESS) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    event.operation = L"RpcClientCall";
    event.phase = COMTRACE_RPC_TIMELINE_PHASE_STOP;
    event.timestamp = 20ULL;
    if (comtrace_rpc_timeline_add_event(&timeline, &event) !=
        COMTRACE_RPC_TIMELINE_SUCCESS) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    for (index = 0U; index < 2U; ++index) {
        event.operation = L"RpcServerCall";
        event.phase = COMTRACE_RPC_TIMELINE_PHASE_START;
        event.process_id = 20U + (DWORD)index;
        event.thread_id = 2U + (DWORD)index;
        event.timestamp = 30ULL + (ULONGLONG)index;
        if (comtrace_rpc_timeline_add_event(&timeline, &event) !=
            COMTRACE_RPC_TIMELINE_SUCCESS) {
            comtrace_rpc_timeline_destroy(&timeline);
            return 0;
        }
        event.phase = COMTRACE_RPC_TIMELINE_PHASE_STOP;
        event.timestamp = 40ULL + (ULONGLONG)index;
        if (comtrace_rpc_timeline_add_event(&timeline, &event) !=
            COMTRACE_RPC_TIMELINE_SUCCESS) {
            comtrace_rpc_timeline_destroy(&timeline);
            return 0;
        }
    }

    comtrace_rpc_timeline_correlate_calls(&timeline);
    if (timeline.correlated_client_server_calls != 0ULL ||
        timeline.ambiguous_client_server_calls != 1ULL) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }
    for (index = 0U; index < timeline.record_count; ++index) {
        if (timeline.records[index].has_correlated_record) {
            comtrace_rpc_timeline_destroy(&timeline);
            return 0;
        }
    }

    comtrace_rpc_timeline_destroy(&timeline);
    return 1;
}

static int test_failure_status_and_invalid_duration(void)
{
    COMTRACE_RPC_TIMELINE timeline;
    COMTRACE_ACTIVITY_IDS ids;
    COMTRACE_RPC_TIMELINE_EVENT event;
    const COMTRACE_RPC_TIMELINE_RECORD *record;

    comtrace_rpc_timeline_init(&timeline);
    ZeroMemory(&ids, sizeof(ids));
    ids.activity_id = activity_id;
    ids.has_activity_id = TRUE;
    ZeroMemory(&event, sizeof(event));
    event.operation = L"RpcClientCall";
    event.phase = COMTRACE_RPC_TIMELINE_PHASE_START;
    event.process_id = 10U;
    event.thread_id = 20U;
    event.timestamp = 500ULL;
    event.activity_ids = &ids;
    if (comtrace_rpc_timeline_add_event(&timeline, &event) !=
        COMTRACE_RPC_TIMELINE_SUCCESS) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    event.phase = COMTRACE_RPC_TIMELINE_PHASE_STOP;
    event.timestamp = 400ULL;
    event.has_status = TRUE;
    event.status = 1726U;
    if (comtrace_rpc_timeline_add_event(&timeline, &event) !=
        COMTRACE_RPC_TIMELINE_SUCCESS) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    record = comtrace_rpc_timeline_get(&timeline, 0U);
    if (record == NULL ||
        !record->has_stop ||
        !record->invalid_duration_order ||
        record->has_duration ||
        !record->has_status ||
        !record->status_is_failure ||
        record->status != 1726U ||
        timeline.invalid_duration_orders != 1ULL) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    comtrace_rpc_timeline_destroy(&timeline);
    return 1;
}

static int test_related_activity_id_pair(void)
{
    COMTRACE_RPC_TIMELINE timeline;
    COMTRACE_ACTIVITY_IDS ids;
    COMTRACE_RPC_TIMELINE_EVENT event;
    const COMTRACE_RPC_TIMELINE_RECORD *record;

    comtrace_rpc_timeline_init(&timeline);
    ZeroMemory(&ids, sizeof(ids));
    ids.related_activity_id = activity_id;
    ids.has_related_activity_id = TRUE;
    ZeroMemory(&event, sizeof(event));
    event.operation = L"RpcServerCall";
    event.phase = COMTRACE_RPC_TIMELINE_PHASE_START;
    event.process_id = 30U;
    event.thread_id = 40U;
    event.timestamp = 100ULL;
    event.activity_ids = &ids;
    if (comtrace_rpc_timeline_add_event(&timeline, &event) !=
        COMTRACE_RPC_TIMELINE_SUCCESS) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    event.phase = COMTRACE_RPC_TIMELINE_PHASE_STOP;
    event.process_id = 31U;
    event.thread_id = 41U;
    event.timestamp = 150ULL;
    if (comtrace_rpc_timeline_add_event(&timeline, &event) !=
        COMTRACE_RPC_TIMELINE_SUCCESS) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    record = comtrace_rpc_timeline_get(&timeline, 0U);
    if (record == NULL ||
        !record->has_stop ||
        !record->has_duration ||
        record->duration_ticks != 50ULL ||
        record->match_type != COMTRACE_RPC_TIMELINE_MATCH_RELATED_ACTIVITY_ID) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    comtrace_rpc_timeline_destroy(&timeline);
    return 1;
}

static int test_ambiguous_cross_thread_stop_is_not_guessed(void)
{
    COMTRACE_RPC_TIMELINE timeline;
    COMTRACE_ACTIVITY_IDS ids;
    COMTRACE_RPC_TIMELINE_EVENT event;
    const COMTRACE_RPC_TIMELINE_RECORD *stop_record;

    comtrace_rpc_timeline_init(&timeline);
    ZeroMemory(&ids, sizeof(ids));
    ids.activity_id = activity_id;
    ids.has_activity_id = TRUE;
    ZeroMemory(&event, sizeof(event));
    event.operation = L"RpcClientCall";
    event.phase = COMTRACE_RPC_TIMELINE_PHASE_START;
    event.process_id = 10U;
    event.thread_id = 1U;
    event.timestamp = 100ULL;
    event.activity_ids = &ids;
    if (comtrace_rpc_timeline_add_event(&timeline, &event) !=
        COMTRACE_RPC_TIMELINE_SUCCESS) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    event.process_id = 11U;
    event.thread_id = 2U;
    event.timestamp = 110ULL;
    if (comtrace_rpc_timeline_add_event(&timeline, &event) !=
        COMTRACE_RPC_TIMELINE_SUCCESS) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    event.phase = COMTRACE_RPC_TIMELINE_PHASE_STOP;
    event.process_id = 12U;
    event.thread_id = 3U;
    event.timestamp = 120ULL;
    if (comtrace_rpc_timeline_add_event(&timeline, &event) !=
        COMTRACE_RPC_TIMELINE_SUCCESS) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    stop_record = comtrace_rpc_timeline_get(&timeline, 2U);
    if (stop_record == NULL ||
        !stop_record->ambiguous_match ||
        stop_record->has_start ||
        timeline.pending_count != 2U ||
        timeline.unmatched_stops != 1ULL ||
        timeline.ambiguous_stops != 1ULL) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    comtrace_rpc_timeline_destroy(&timeline);
    return 1;
}

static int test_same_thread_rpc_match_is_preferred(void)
{
    COMTRACE_RPC_TIMELINE timeline;
    COMTRACE_ACTIVITY_IDS ids;
    COMTRACE_RPC_TIMELINE_EVENT event;
    const COMTRACE_RPC_TIMELINE_RECORD *first;
    const COMTRACE_RPC_TIMELINE_RECORD *second;

    comtrace_rpc_timeline_init(&timeline);
    ZeroMemory(&ids, sizeof(ids));
    ids.activity_id = activity_id;
    ids.has_activity_id = TRUE;
    ZeroMemory(&event, sizeof(event));
    event.operation = L"RpcServerCall";
    event.phase = COMTRACE_RPC_TIMELINE_PHASE_START;
    event.process_id = 10U;
    event.thread_id = 1U;
    event.timestamp = 100ULL;
    event.activity_ids = &ids;
    if (comtrace_rpc_timeline_add_event(&timeline, &event) !=
        COMTRACE_RPC_TIMELINE_SUCCESS) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    event.process_id = 11U;
    event.thread_id = 2U;
    event.timestamp = 110ULL;
    if (comtrace_rpc_timeline_add_event(&timeline, &event) !=
        COMTRACE_RPC_TIMELINE_SUCCESS) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    event.phase = COMTRACE_RPC_TIMELINE_PHASE_STOP;
    event.timestamp = 120ULL;
    if (comtrace_rpc_timeline_add_event(&timeline, &event) !=
        COMTRACE_RPC_TIMELINE_SUCCESS) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    first = comtrace_rpc_timeline_get(&timeline, 0U);
    second = comtrace_rpc_timeline_get(&timeline, 1U);
    if (first == NULL || second == NULL ||
        first->has_stop ||
        !second->has_stop ||
        second->stop_process_id != 11U ||
        second->stop_thread_id != 2U ||
        second->ambiguous_match ||
        timeline.pending_count != 1U ||
        timeline.ambiguous_stops != 0ULL) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    comtrace_rpc_timeline_destroy(&timeline);
    return 1;
}

int main(void)
{
    if (!test_activity_pair() ||
        !test_fallback_and_one_sided_identifiers() ||
        !test_activity_id_pair_across_process_and_thread() ||
        !test_cross_process_client_server_correlation() ||
        !test_ambiguous_client_server_correlation_is_not_guessed() ||
        !test_failure_status_and_invalid_duration() ||
        !test_related_activity_id_pair() ||
        !test_ambiguous_cross_thread_stop_is_not_guessed() ||
        !test_same_thread_rpc_match_is_preferred()) {
        fputs("RPC timeline tests failed.\n", stderr);
        return 1;
    }

    puts("RPC timeline tests passed.");
    return 0;
}
