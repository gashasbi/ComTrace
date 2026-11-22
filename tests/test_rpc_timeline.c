#include "rpc_timeline.h"

#include <stdio.h>
#include <string.h>

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

int main(void)
{
    if (!test_activity_pair() ||
        !test_fallback_and_one_sided_identifiers()) {
        fputs("RPC timeline tests failed.\n", stderr);
        return 1;
    }

    puts("RPC timeline tests passed.");
    return 0;
}
