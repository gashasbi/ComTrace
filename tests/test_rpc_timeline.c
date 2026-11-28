#include "rpc_timeline.h"

#include <stdio.h>
#include <stdlib.h>
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


static int test_related_activity_id_client_server_correlation(void)
{
    COMTRACE_RPC_TIMELINE timeline;
    COMTRACE_RPC_TIMELINE_RECORD *records;
    GUID activities[7];
    size_t index;

    for (index = 0U; index < _countof(activities); ++index) {
        activities[index] = activity_id;
        activities[index].Data1 += (ULONG)(index + 1U);
    }

    comtrace_rpc_timeline_init(&timeline);
    records = (COMTRACE_RPC_TIMELINE_RECORD *)calloc(9U, sizeof(*records));
    if (records == NULL) {
        return 0;
    }

    timeline.records = records;
    timeline.record_count = 9U;
    timeline.record_capacity = 9U;

    for (index = 0U; index < timeline.record_count; ++index) {
        records[index].has_start = TRUE;
        records[index].has_stop = TRUE;
        records[index].has_activity_id = TRUE;
        records[index].has_interface_uuid = TRUE;
        records[index].interface_uuid = interface_id;
        records[index].has_procedure_number = TRUE;
        records[index].procedure_number = 7U;
        records[index].role = (index == 0U || index == 2U ||
            index == 4U || index == 7U)
            ? COMTRACE_RPC_ROLE_CLIENT
            : COMTRACE_RPC_ROLE_SERVER;
    }

    records[0].activity_id = activities[0];
    records[0].has_related_activity_id = TRUE;
    records[0].related_activity_id = activities[1];

    records[1].activity_id = activities[1];

    records[2].activity_id = activities[2];

    records[3].activity_id = activities[3];
    records[3].has_related_activity_id = TRUE;
    records[3].related_activity_id = activities[2];

    records[4].activity_id = activities[4];
    records[4].has_related_activity_id = TRUE;
    records[4].related_activity_id = activities[5];

    records[5].activity_id = activities[5];
    records[6].activity_id = activities[5];

    records[7].activity_id = activities[6];
    records[7].has_related_activity_id = TRUE;
    records[7].related_activity_id = activities[6];

    records[8].activity_id = activities[6];
    records[8].has_related_activity_id = TRUE;
    records[8].related_activity_id = activities[6];

    comtrace_rpc_timeline_correlate_calls(&timeline);

    if (!records[0].has_correlated_record ||
        records[0].correlated_record_index != 1U ||
        records[0].correlation_type != COMTRACE_RPC_CORRELATION_RELATED_ACTIVITY_ID ||
        !records[1].has_correlated_record ||
        records[1].correlated_record_index != 0U ||
        records[1].correlation_type != COMTRACE_RPC_CORRELATION_RELATED_ACTIVITY_ID ||
        !records[2].has_correlated_record ||
        records[2].correlated_record_index != 3U ||
        records[2].correlation_type != COMTRACE_RPC_CORRELATION_RELATED_ACTIVITY_ID ||
        !records[3].has_correlated_record ||
        records[3].correlated_record_index != 2U ||
        records[3].correlation_type != COMTRACE_RPC_CORRELATION_RELATED_ACTIVITY_ID ||
        records[4].has_correlated_record ||
        records[5].has_correlated_record ||
        records[6].has_correlated_record ||
        !records[7].has_correlated_record ||
        records[7].correlated_record_index != 8U ||
        records[7].correlation_type != COMTRACE_RPC_CORRELATION_ACTIVITY_ID ||
        !records[8].has_correlated_record ||
        records[8].correlated_record_index != 7U ||
        records[8].correlation_type != COMTRACE_RPC_CORRELATION_ACTIVITY_ID ||
        timeline.correlated_client_server_calls != 3ULL ||
        timeline.ambiguous_client_server_calls != 1ULL) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    comtrace_rpc_timeline_destroy(&timeline);
    return 1;
}

static int test_large_rpc_client_server_correlation(void)
{
    const size_t record_count = 46113U;
    COMTRACE_RPC_TIMELINE timeline;
    COMTRACE_RPC_TIMELINE_RECORD *records;
    size_t index;

    comtrace_rpc_timeline_init(&timeline);
    records = (COMTRACE_RPC_TIMELINE_RECORD *)calloc(
        record_count,
        sizeof(*records)
    );
    if (records == NULL) {
        return 0;
    }

    timeline.records = records;
    timeline.record_count = record_count;
    timeline.record_capacity = record_count;

    for (index = 0U; index < record_count; ++index) {
        records[index].has_start = TRUE;
        records[index].has_stop = TRUE;
        records[index].has_activity_id = TRUE;
        records[index].activity_id = activity_id;
        records[index].activity_id.Data1 = (ULONG)(index + 1U);
        records[index].has_interface_uuid = TRUE;
        records[index].interface_uuid = interface_id;
        records[index].has_procedure_number = TRUE;
        records[index].procedure_number = 7U;
        records[index].role = index % 2U == 0U
            ? COMTRACE_RPC_ROLE_CLIENT
            : COMTRACE_RPC_ROLE_SERVER;
    }

    comtrace_rpc_timeline_correlate_calls(&timeline);

    if (timeline.correlated_client_server_calls != 0ULL ||
        timeline.ambiguous_client_server_calls != 0ULL) {
        comtrace_rpc_timeline_destroy(&timeline);
        return 0;
    }

    for (index = 0U; index < record_count; ++index) {
        if (records[index].has_correlated_record) {
            comtrace_rpc_timeline_destroy(&timeline);
            return 0;
        }
    }

    comtrace_rpc_timeline_destroy(&timeline);
    return 1;
}

static int test_com_operation_link_index(void)
{
    COMTRACE_RPC_TIMELINE rpc_timeline;
    COMTRACE_TIMELINE com_timeline;
    COMTRACE_RPC_TIMELINE_RECORD *rpc_records;
    COMTRACE_TIMELINE_RECORD *com_records;
    GUID second_activity = activity_id;
    GUID third_activity = activity_id;
    size_t index;

    second_activity.Data1++;
    third_activity.Data1 += 2U;
    comtrace_rpc_timeline_init(&rpc_timeline);
    ZeroMemory(&com_timeline, sizeof(com_timeline));

    rpc_records = (COMTRACE_RPC_TIMELINE_RECORD *)calloc(4U, sizeof(*rpc_records));
    com_records = (COMTRACE_TIMELINE_RECORD *)calloc(4U, sizeof(*com_records));
    if (rpc_records == NULL || com_records == NULL) {
        free(rpc_records);
        free(com_records);
        return 0;
    }

    rpc_timeline.records = rpc_records;
    rpc_timeline.record_count = 4U;
    rpc_timeline.record_capacity = 4U;
    com_timeline.records = com_records;
    com_timeline.record_count = 4U;
    com_timeline.record_capacity = 4U;

    for (index = 0U; index < 4U; ++index) {
        rpc_records[index].has_com_operation = TRUE;
        rpc_records[index].com_operation_index = 99U;
        rpc_records[index].com_operation_match_ambiguous = TRUE;
        rpc_records[index].has_start = TRUE;
        rpc_records[index].has_activity_id = TRUE;
        rpc_records[index].has_interface_uuid = TRUE;
    }

    rpc_records[0].activity_id = activity_id;
    rpc_records[0].interface_uuid = interface_id;
    rpc_records[1].activity_id = second_activity;
    rpc_records[1].interface_uuid = interface_id;
    rpc_records[2].activity_id = third_activity;
    rpc_records[2].interface_uuid = interface_id;
    rpc_records[3].has_start = FALSE;

    for (index = 0U; index < 4U; ++index) {
        com_records[index].has_start = TRUE;
        com_records[index].has_activity_ids = TRUE;
        com_records[index].activity_ids.has_activity_id = TRUE;
        com_records[index].has_object_identity = TRUE;
        com_records[index].object_identity.has_iid = TRUE;
    }

    com_records[0].activity_ids.activity_id = activity_id;
    com_records[0].object_identity.iid = interface_id;
    com_records[1].activity_ids.activity_id = second_activity;
    com_records[1].object_identity.iid = interface_id;
    com_records[2].activity_ids.activity_id = second_activity;
    com_records[2].object_identity.iid = interface_id;
    com_records[3].activity_ids.activity_id = activity_id;
    com_records[3].object_identity.iid = second_activity;

    if (comtrace_rpc_timeline_link_com_operations(
            &rpc_timeline,
            &com_timeline
        ) != COMTRACE_RPC_TIMELINE_SUCCESS ||
        !rpc_records[0].has_com_operation ||
        rpc_records[0].com_operation_index != 0U ||
        rpc_records[0].com_operation_match_ambiguous ||
        rpc_records[1].has_com_operation ||
        !rpc_records[1].com_operation_match_ambiguous ||
        rpc_records[2].has_com_operation ||
        rpc_records[2].com_operation_match_ambiguous ||
        rpc_records[3].has_com_operation ||
        rpc_records[3].com_operation_match_ambiguous ||
        rpc_records[3].com_operation_index != 0U) {
        comtrace_rpc_timeline_destroy(&rpc_timeline);
        free(com_records);
        return 0;
    }

    comtrace_rpc_timeline_destroy(&rpc_timeline);
    free(com_records);
    return 1;
}

int main(void)
{
    if (!test_related_activity_id_client_server_correlation()) {
        puts("test_related_activity_id_client_server_correlation failed.");
        return 1;
    }
    if (!test_large_rpc_client_server_correlation()) {
        puts("test_large_rpc_client_server_correlation failed.");
        return 1;
    }
    if (!test_com_operation_link_index()) {
        puts("test_com_operation_link_index failed.");
        return 1;
    }
    if (!test_activity_pair()) {
        puts("test_activity_pair failed.");
        return 1;
    }
    if (!test_fallback_and_one_sided_identifiers()) {
        puts("test_fallback_and_one_sided_identifiers failed.");
        return 1;
    }
    if (!test_activity_id_pair_across_process_and_thread()) {
        puts("test_activity_id_pair_across_process_and_thread failed.");
        return 1;
    }
    if (!test_cross_process_client_server_correlation()) {
        puts("test_cross_process_client_server_correlation failed.");
        return 1;
    }
    if (!test_ambiguous_client_server_correlation_is_not_guessed()) {
        puts("test_ambiguous_client_server_correlation_is_not_guessed failed.");
        return 1;
    }
    if (!test_failure_status_and_invalid_duration()) {
        puts("test_failure_status_and_invalid_duration failed.");
        return 1;
    }
    if (!test_related_activity_id_pair()) {
        puts("test_related_activity_id_pair failed.");
        return 1;
    }
    if (!test_ambiguous_cross_thread_stop_is_not_guessed()) {
        puts("test_ambiguous_cross_thread_stop_is_not_guessed failed.");
        return 1;
    }
    if (!test_same_thread_rpc_match_is_preferred()) {
        puts("test_same_thread_rpc_match_is_preferred failed.");
        return 1;
    }

    puts("RPC timeline tests passed.");
    return 0;
}
