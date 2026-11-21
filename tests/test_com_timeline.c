#include <stdio.h>
#include <string.h>
#include "com_timeline.h"

static const GUID activity_one = {
    0x10213243, 0x5465, 0x7687,
    { 0x98, 0xa9, 0xba, 0xcb, 0xdc, 0xed, 0xfe, 0x0f }
};

static const GUID activity_two = {
    0x20314253, 0x6475, 0x8697,
    { 0xa8, 0xb9, 0xca, 0xdb, 0xec, 0xfd, 0x0e, 0x1f }
};

static int test_matched_operation(void)
{
    COMTRACE_TIMELINE timeline;
    COMTRACE_TIMELINE_EVENT event;
    COMTRACE_TIMELINE_RECORD const *record;
    COMTRACE_COM_OBJECT_IDENTITY identity;
    COMTRACE_TIMELINE_STATUS status;

    comtrace_timeline_init(&timeline);
    ZeroMemory(&event, sizeof(event));
    ZeroMemory(&identity, sizeof(identity));

    identity.has_oid = TRUE;
    identity.oid = 0x1122334455667788ULL;

    event.operation = L"COM_ClientSyncCall";
    event.phase = COMTRACE_TIMELINE_PHASE_START;
    event.has_call_trace_id = TRUE;
    event.call_trace_id = 0xAABBCCDDULL;
    event.process_id = 100U;
    event.thread_id = 200U;
    event.timestamp = 1000ULL;
    event.object_identity = &identity;

    status = comtrace_timeline_add_event(&timeline, &event);
    if (status != COMTRACE_TIMELINE_SUCCESS) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    event.phase = COMTRACE_TIMELINE_PHASE_STOP;
    event.timestamp = 1450ULL;
    event.has_hresult = TRUE;
    event.hresult = S_OK;

    status = comtrace_timeline_add_event(&timeline, &event);
    record = comtrace_timeline_get(&timeline, 0U);

    if (status != COMTRACE_TIMELINE_SUCCESS ||
        comtrace_timeline_count(&timeline) != 1U ||
        comtrace_timeline_pending_count(&timeline) != 0U ||
        record == NULL ||
        !record->has_start ||
        !record->has_stop ||
        !record->has_duration ||
        record->duration_ticks != 450ULL ||
        record->match_type != COMTRACE_TIMELINE_MATCH_CALL_TRACE_ID ||
        !record->has_hresult ||
        record->hresult != S_OK ||
        !record->has_object_identity ||
        record->object_identity.oid != identity.oid) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    comtrace_timeline_destroy(&timeline);
    return 1;
}

static int test_incomplete_and_unmatched_operations(void)
{
    COMTRACE_TIMELINE timeline;
    COMTRACE_TIMELINE_EVENT event;
    const COMTRACE_TIMELINE_RECORD *record;
    COMTRACE_TIMELINE_STATUS status;

    comtrace_timeline_init(&timeline);
    ZeroMemory(&event, sizeof(event));

    event.operation = L"COM_ServerSyncCall";
    event.phase = COMTRACE_TIMELINE_PHASE_START;
    event.has_call_trace_id = TRUE;
    event.call_trace_id = 10ULL;
    event.process_id = 50U;
    event.thread_id = 60U;
    event.timestamp = 100ULL;

    status = comtrace_timeline_add_event(&timeline, &event);
    if (status != COMTRACE_TIMELINE_SUCCESS) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    event.phase = COMTRACE_TIMELINE_PHASE_STOP;
    event.call_trace_id = 11ULL;
    event.timestamp = 200ULL;

    status = comtrace_timeline_add_event(&timeline, &event);
    if (status != COMTRACE_TIMELINE_SUCCESS) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    event.phase = COMTRACE_TIMELINE_PHASE_START;
    event.has_call_trace_id = FALSE;
    event.timestamp = 300ULL;

    status = comtrace_timeline_add_event(&timeline, &event);
    record = comtrace_timeline_get(&timeline, 0U);

    if (status != COMTRACE_TIMELINE_SUCCESS ||
        comtrace_timeline_count(&timeline) != 3U ||
        comtrace_timeline_pending_count(&timeline) != 2U ||
        timeline.unmatched_stops != 1ULL ||
        timeline.missing_call_trace_ids != 1ULL ||
        timeline.missing_activity_ids != 3ULL ||
        record == NULL ||
        !record->has_start ||
        record->has_stop) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    record = comtrace_timeline_get(&timeline, 1U);
    if (record == NULL || record->has_start || !record->has_stop) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    record = comtrace_timeline_get(&timeline, 2U);
    if (record == NULL || !record->has_start || record->has_call_trace_id) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    comtrace_timeline_destroy(&timeline);
    return 1;
}

static int test_mismatched_keys_remain_unmatched(void)
{
    COMTRACE_TIMELINE timeline;
    COMTRACE_TIMELINE_EVENT event;
    const COMTRACE_TIMELINE_RECORD *record;

    comtrace_timeline_init(&timeline);
    ZeroMemory(&event, sizeof(event));

    event.operation = L"COM_ClientSyncCall";
    event.phase = COMTRACE_TIMELINE_PHASE_START;
    event.has_call_trace_id = TRUE;
    event.call_trace_id = 55ULL;
    event.process_id = 100U;
    event.thread_id = 200U;
    event.timestamp = 1000ULL;

    if (comtrace_timeline_add_event(&timeline, &event) !=
        COMTRACE_TIMELINE_SUCCESS) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    event.phase = COMTRACE_TIMELINE_PHASE_STOP;
    event.process_id = 101U;
    event.timestamp = 1200ULL;

    if (comtrace_timeline_add_event(&timeline, &event) !=
        COMTRACE_TIMELINE_SUCCESS ||
        comtrace_timeline_pending_count(&timeline) != 1U ||
        comtrace_timeline_count(&timeline) != 2U) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    event.process_id = 100U;
    event.operation = L"COM_ServerSyncCall";
    event.timestamp = 1300ULL;

    if (comtrace_timeline_add_event(&timeline, &event) !=
        COMTRACE_TIMELINE_SUCCESS ||
        comtrace_timeline_pending_count(&timeline) != 1U ||
        comtrace_timeline_count(&timeline) != 3U) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    event.operation = L"COM_ClientSyncCall";
    event.thread_id = 201U;

    if (comtrace_timeline_add_event(&timeline, &event) !=
        COMTRACE_TIMELINE_SUCCESS ||
        comtrace_timeline_pending_count(&timeline) != 1U ||
        comtrace_timeline_count(&timeline) != 4U) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    event.thread_id = 200U;
    event.timestamp = 1400ULL;

    if (comtrace_timeline_add_event(&timeline, &event) !=
        COMTRACE_TIMELINE_SUCCESS ||
        comtrace_timeline_pending_count(&timeline) != 0U ||
        comtrace_timeline_count(&timeline) != 4U) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    record = comtrace_timeline_get(&timeline, 0U);
    if (record == NULL || !record->has_duration ||
        record->duration_ticks != 400ULL) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    record = comtrace_timeline_get(&timeline, 1U);
    if (record == NULL || record->has_start || !record->has_stop) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    comtrace_timeline_destroy(&timeline);
    return 1;
}

static int test_out_of_order_timestamps(void)
{
    COMTRACE_TIMELINE timeline;
    COMTRACE_TIMELINE_EVENT event;
    const COMTRACE_TIMELINE_RECORD *record;

    comtrace_timeline_init(&timeline);
    ZeroMemory(&event, sizeof(event));

    event.operation = L"COM_ClientAsyncCall";
    event.phase = COMTRACE_TIMELINE_PHASE_START;
    event.has_call_trace_id = TRUE;
    event.call_trace_id = 77ULL;
    event.process_id = 10U;
    event.thread_id = 20U;
    event.timestamp = 500ULL;

    if (comtrace_timeline_add_event(&timeline, &event) !=
        COMTRACE_TIMELINE_SUCCESS) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    event.phase = COMTRACE_TIMELINE_PHASE_STOP;
    event.timestamp = 400ULL;

    if (comtrace_timeline_add_event(&timeline, &event) !=
        COMTRACE_TIMELINE_SUCCESS) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    record = comtrace_timeline_get(&timeline, 0U);
    if (record == NULL ||
        !record->has_start ||
        !record->has_stop ||
        record->has_duration) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    comtrace_timeline_destroy(&timeline);
    return 1;
}


static int test_activity_id_pairing(void)
{
    COMTRACE_TIMELINE timeline;
    COMTRACE_TIMELINE_EVENT event;
    COMTRACE_ACTIVITY_IDS start_ids;
    COMTRACE_ACTIVITY_IDS stop_ids;
    const COMTRACE_TIMELINE_RECORD *record;

    comtrace_timeline_init(&timeline);
    ZeroMemory(&event, sizeof(event));
    ZeroMemory(&start_ids, sizeof(start_ids));
    ZeroMemory(&stop_ids, sizeof(stop_ids));

    start_ids.has_activity_id = TRUE;
    start_ids.activity_id = activity_one;
    stop_ids.has_activity_id = TRUE;
    stop_ids.activity_id = activity_one;
    stop_ids.has_related_activity_id = TRUE;
    stop_ids.related_activity_id = activity_two;

    event.operation = L"COM_ServerSyncCall";
    event.phase = COMTRACE_TIMELINE_PHASE_START;
    event.process_id = 100U;
    event.thread_id = 200U;
    event.timestamp = 1000ULL;
    event.activity_ids = &start_ids;

    if (comtrace_timeline_add_event(&timeline, &event) !=
        COMTRACE_TIMELINE_SUCCESS) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    event.phase = COMTRACE_TIMELINE_PHASE_STOP;
    event.timestamp = 1450ULL;
    event.activity_ids = &stop_ids;

    if (comtrace_timeline_add_event(&timeline, &event) !=
        COMTRACE_TIMELINE_SUCCESS) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    record = comtrace_timeline_get(&timeline, 0U);
    if (record == NULL ||
        !record->has_start ||
        !record->has_stop ||
        !record->has_duration ||
        record->duration_ticks != 450ULL ||
        record->match_type != COMTRACE_TIMELINE_MATCH_ACTIVITY_ID ||
        record->has_call_trace_id ||
        !record->has_activity_ids ||
        !record->activity_ids.has_activity_id ||
        !record->activity_ids.has_related_activity_id ||
        memcmp(&record->activity_ids.activity_id, &activity_one, sizeof(GUID)) != 0 ||
        memcmp(&record->activity_ids.related_activity_id, &activity_two, sizeof(GUID)) != 0 ||
        comtrace_timeline_pending_count(&timeline) != 0U ||
        timeline.missing_call_trace_ids != 2ULL ||
        timeline.missing_activity_ids != 0ULL) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    comtrace_timeline_destroy(&timeline);
    return 1;
}

static int test_activity_id_mismatch_remains_unmatched(void)
{
    COMTRACE_TIMELINE timeline;
    COMTRACE_TIMELINE_EVENT event;
    COMTRACE_ACTIVITY_IDS start_ids;
    COMTRACE_ACTIVITY_IDS stop_ids;
    const COMTRACE_TIMELINE_RECORD *record;

    comtrace_timeline_init(&timeline);
    ZeroMemory(&event, sizeof(event));
    ZeroMemory(&start_ids, sizeof(start_ids));
    ZeroMemory(&stop_ids, sizeof(stop_ids));

    start_ids.has_activity_id = TRUE;
    start_ids.activity_id = activity_one;
    stop_ids.has_activity_id = TRUE;
    stop_ids.activity_id = activity_two;

    event.operation = L"COM_ClientSyncCall";
    event.phase = COMTRACE_TIMELINE_PHASE_START;
    event.process_id = 50U;
    event.thread_id = 60U;
    event.timestamp = 100ULL;
    event.activity_ids = &start_ids;

    if (comtrace_timeline_add_event(&timeline, &event) !=
        COMTRACE_TIMELINE_SUCCESS) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    event.phase = COMTRACE_TIMELINE_PHASE_STOP;
    event.timestamp = 200ULL;
    event.activity_ids = &stop_ids;

    if (comtrace_timeline_add_event(&timeline, &event) !=
        COMTRACE_TIMELINE_SUCCESS ||
        comtrace_timeline_count(&timeline) != 2U ||
        comtrace_timeline_pending_count(&timeline) != 1U ||
        timeline.unmatched_stops != 1ULL) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    record = comtrace_timeline_get(&timeline, 0U);
    if (record == NULL || !record->has_start || record->has_stop) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    record = comtrace_timeline_get(&timeline, 1U);
    if (record == NULL || record->has_start || !record->has_stop ||
        record->match_type != COMTRACE_TIMELINE_MATCH_NONE) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    comtrace_timeline_destroy(&timeline);
    return 1;
}

static int test_activity_id_pairing_uses_lifo_order(void)
{
    COMTRACE_TIMELINE timeline;
    COMTRACE_TIMELINE_EVENT event;
    COMTRACE_ACTIVITY_IDS activity_ids;
    const COMTRACE_TIMELINE_RECORD *first;
    const COMTRACE_TIMELINE_RECORD *second;

    comtrace_timeline_init(&timeline);
    ZeroMemory(&event, sizeof(event));
    ZeroMemory(&activity_ids, sizeof(activity_ids));

    activity_ids.has_activity_id = TRUE;
    activity_ids.activity_id = activity_one;
    event.operation = L"COM_GetClassFactory";
    event.process_id = 50U;
    event.thread_id = 60U;
    event.timestamp = 100ULL;
    event.activity_ids = &activity_ids;
    event.phase = COMTRACE_TIMELINE_PHASE_START;

    if (comtrace_timeline_add_event(&timeline, &event) !=
        COMTRACE_TIMELINE_SUCCESS) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    event.timestamp = 110ULL;
    if (comtrace_timeline_add_event(&timeline, &event) !=
        COMTRACE_TIMELINE_SUCCESS) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    event.phase = COMTRACE_TIMELINE_PHASE_STOP;
    event.timestamp = 120ULL;
    if (comtrace_timeline_add_event(&timeline, &event) !=
        COMTRACE_TIMELINE_SUCCESS) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    event.timestamp = 130ULL;
    if (comtrace_timeline_add_event(&timeline, &event) !=
        COMTRACE_TIMELINE_SUCCESS) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    first = comtrace_timeline_get(&timeline, 0U);
    second = comtrace_timeline_get(&timeline, 1U);
    if (first == NULL || second == NULL ||
        !first->has_start || !first->has_stop ||
        !second->has_start || !second->has_stop ||
        first->duration_ticks != 30ULL ||
        second->duration_ticks != 10ULL ||
        first->match_type != COMTRACE_TIMELINE_MATCH_ACTIVITY_ID ||
        second->match_type != COMTRACE_TIMELINE_MATCH_ACTIVITY_ID ||
        comtrace_timeline_pending_count(&timeline) != 0U) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    comtrace_timeline_destroy(&timeline);
    return 1;
}

static int test_activity_id_fallback_rejects_one_sided_call_trace_id(void)
{
    COMTRACE_TIMELINE timeline;
    COMTRACE_TIMELINE_EVENT event;
    COMTRACE_ACTIVITY_IDS activity_ids;

    comtrace_timeline_init(&timeline);
    ZeroMemory(&event, sizeof(event));
    ZeroMemory(&activity_ids, sizeof(activity_ids));

    activity_ids.has_activity_id = TRUE;
    activity_ids.activity_id = activity_one;
    event.operation = L"COM_ServerSyncCall";
    event.phase = COMTRACE_TIMELINE_PHASE_START;
    event.has_call_trace_id = TRUE;
    event.call_trace_id = 77ULL;
    event.process_id = 10U;
    event.thread_id = 20U;
    event.timestamp = 100ULL;
    event.activity_ids = &activity_ids;

    if (comtrace_timeline_add_event(&timeline, &event) !=
        COMTRACE_TIMELINE_SUCCESS) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    event.phase = COMTRACE_TIMELINE_PHASE_STOP;
    event.has_call_trace_id = FALSE;
    event.timestamp = 200ULL;

    if (comtrace_timeline_add_event(&timeline, &event) !=
        COMTRACE_TIMELINE_SUCCESS ||
        comtrace_timeline_count(&timeline) != 2U ||
        comtrace_timeline_pending_count(&timeline) != 1U ||
        timeline.unmatched_stops != 1ULL ||
        timeline.missing_call_trace_ids != 1ULL ||
        timeline.missing_activity_ids != 0ULL) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    comtrace_timeline_destroy(&timeline);
    return 1;
}

static int test_missing_activity_ids_use_process_thread_fallback(void)
{
    COMTRACE_TIMELINE timeline;
    COMTRACE_TIMELINE_EVENT event;
    const COMTRACE_TIMELINE_RECORD *record;

    comtrace_timeline_init(&timeline);
    ZeroMemory(&event, sizeof(event));

    event.operation = L"COM_ClientSyncCall";
    event.phase = COMTRACE_TIMELINE_PHASE_START;
    event.process_id = 10U;
    event.thread_id = 20U;
    event.timestamp = 100ULL;

    if (comtrace_timeline_add_event(&timeline, &event) !=
        COMTRACE_TIMELINE_SUCCESS) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    event.phase = COMTRACE_TIMELINE_PHASE_STOP;
    event.timestamp = 200ULL;

    if (comtrace_timeline_add_event(&timeline, &event) !=
        COMTRACE_TIMELINE_SUCCESS ||
        timeline.record_count != 1U ||
        timeline.pending_count != 0U ||
        timeline.unmatched_stops != 0ULL ||
        timeline.missing_call_trace_ids != 2ULL ||
        timeline.missing_activity_ids != 2ULL) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    record = comtrace_timeline_get(&timeline, 0U);
    if (record == NULL ||
        !record->has_start ||
        !record->has_stop ||
        !record->has_duration ||
        record->duration_ticks != 100ULL ||
        record->match_type != COMTRACE_TIMELINE_MATCH_PROCESS_THREAD) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    comtrace_timeline_destroy(&timeline);
    return 1;
}

static int test_one_sided_activity_id_does_not_pair(void)
{
    COMTRACE_TIMELINE timeline;
    COMTRACE_TIMELINE_EVENT event;
    COMTRACE_ACTIVITY_IDS activity_ids;
    const COMTRACE_TIMELINE_RECORD *start_record;
    const COMTRACE_TIMELINE_RECORD *stop_record;

    comtrace_timeline_init(&timeline);
    ZeroMemory(&event, sizeof(event));
    ZeroMemory(&activity_ids, sizeof(activity_ids));

    activity_ids.has_activity_id = TRUE;
    activity_ids.activity_id = activity_one;
    event.operation = L"COM_ServerSyncCall";
    event.phase = COMTRACE_TIMELINE_PHASE_START;
    event.process_id = 30U;
    event.thread_id = 40U;
    event.timestamp = 100ULL;
    event.activity_ids = &activity_ids;

    if (comtrace_timeline_add_event(&timeline, &event) !=
        COMTRACE_TIMELINE_SUCCESS) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    event.phase = COMTRACE_TIMELINE_PHASE_STOP;
    event.timestamp = 200ULL;
    event.activity_ids = NULL;

    if (comtrace_timeline_add_event(&timeline, &event) !=
            COMTRACE_TIMELINE_SUCCESS ||
        timeline.record_count != 2U ||
        timeline.pending_count != 1U ||
        timeline.unmatched_stops != 1ULL) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    start_record = comtrace_timeline_get(&timeline, 0U);
    stop_record = comtrace_timeline_get(&timeline, 1U);
    if (start_record == NULL || stop_record == NULL ||
        !start_record->has_start || start_record->has_stop ||
        !stop_record->has_stop ||
        stop_record->match_type != COMTRACE_TIMELINE_MATCH_NONE) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    comtrace_timeline_destroy(&timeline);
    return 1;
}

static int test_process_thread_pairing_uses_lifo_order(void)
{
    COMTRACE_TIMELINE timeline;
    COMTRACE_TIMELINE_EVENT event;
    const COMTRACE_TIMELINE_RECORD *first;
    const COMTRACE_TIMELINE_RECORD *second;

    comtrace_timeline_init(&timeline);
    ZeroMemory(&event, sizeof(event));

    event.operation = L"COM_GetClassFactory";
    event.process_id = 50U;
    event.thread_id = 60U;
    event.timestamp = 100ULL;
    event.phase = COMTRACE_TIMELINE_PHASE_START;

    if (comtrace_timeline_add_event(&timeline, &event) !=
        COMTRACE_TIMELINE_SUCCESS) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    event.timestamp = 110ULL;
    if (comtrace_timeline_add_event(&timeline, &event) !=
        COMTRACE_TIMELINE_SUCCESS) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    event.phase = COMTRACE_TIMELINE_PHASE_STOP;
    event.timestamp = 120ULL;
    if (comtrace_timeline_add_event(&timeline, &event) !=
        COMTRACE_TIMELINE_SUCCESS) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    event.timestamp = 130ULL;
    if (comtrace_timeline_add_event(&timeline, &event) !=
        COMTRACE_TIMELINE_SUCCESS) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    first = comtrace_timeline_get(&timeline, 0U);
    second = comtrace_timeline_get(&timeline, 1U);
    if (first == NULL || second == NULL ||
        !first->has_start || !first->has_stop ||
        !second->has_start || !second->has_stop ||
        first->duration_ticks != 30ULL ||
        second->duration_ticks != 10ULL ||
        first->match_type != COMTRACE_TIMELINE_MATCH_PROCESS_THREAD ||
        second->match_type != COMTRACE_TIMELINE_MATCH_PROCESS_THREAD ||
        timeline.missing_call_trace_ids != 4ULL ||
        timeline.missing_activity_ids != 4ULL ||
        comtrace_timeline_pending_count(&timeline) != 0U) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    comtrace_timeline_destroy(&timeline);
    return 1;
}

static int test_process_thread_fallback_rejects_mismatched_keys(void)
{
    COMTRACE_TIMELINE timeline;
    COMTRACE_TIMELINE_EVENT event;

    comtrace_timeline_init(&timeline);
    ZeroMemory(&event, sizeof(event));

    event.operation = L"COM_ServerSyncCall";
    event.phase = COMTRACE_TIMELINE_PHASE_START;
    event.process_id = 10U;
    event.thread_id = 20U;
    event.timestamp = 100ULL;

    if (comtrace_timeline_add_event(&timeline, &event) !=
        COMTRACE_TIMELINE_SUCCESS) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    event.phase = COMTRACE_TIMELINE_PHASE_STOP;
    event.process_id = 11U;
    event.timestamp = 200ULL;

    if (comtrace_timeline_add_event(&timeline, &event) !=
        COMTRACE_TIMELINE_SUCCESS) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    event.process_id = 10U;
    event.operation = L"COM_ClientSyncCall";
    event.timestamp = 300ULL;

    if (comtrace_timeline_add_event(&timeline, &event) !=
        COMTRACE_TIMELINE_SUCCESS ||
        timeline.record_count != 3U ||
        timeline.pending_count != 1U ||
        timeline.unmatched_stops != 2ULL) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    comtrace_timeline_destroy(&timeline);
    return 1;
}

static int test_invalid_input(void)
{
    COMTRACE_TIMELINE timeline;
    COMTRACE_TIMELINE_EVENT event;

    comtrace_timeline_init(&timeline);
    ZeroMemory(&event, sizeof(event));

    if (comtrace_timeline_add_event(&timeline, NULL) !=
            COMTRACE_TIMELINE_INVALID_ARGUMENT ||
        comtrace_timeline_add_event(&timeline, &event) !=
            COMTRACE_TIMELINE_INVALID_ARGUMENT) {
        comtrace_timeline_destroy(&timeline);
        return 0;
    }

    comtrace_timeline_destroy(&timeline);
    return 1;
}

int main(void)
{
    if (!test_matched_operation() ||
        !test_incomplete_and_unmatched_operations() ||
        !test_mismatched_keys_remain_unmatched() ||
        !test_activity_id_pairing() ||
        !test_activity_id_mismatch_remains_unmatched() ||
        !test_activity_id_pairing_uses_lifo_order() ||
        !test_activity_id_fallback_rejects_one_sided_call_trace_id() ||
        !test_missing_activity_ids_use_process_thread_fallback() ||
        !test_one_sided_activity_id_does_not_pair() ||
        !test_process_thread_pairing_uses_lifo_order() ||
        !test_process_thread_fallback_rejects_mismatched_keys() ||
        !test_out_of_order_timestamps() ||
        !test_invalid_input()) {
        fputs("COM timeline tests failed.\n", stderr);
        return 1;
    }

    puts("COM timeline tests passed.");
    return 0;
}
