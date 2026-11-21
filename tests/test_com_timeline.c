#include <stdio.h>
#include <string.h>
#include "com_timeline.h"

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
        comtrace_timeline_pending_count(&timeline) != 1U ||
        timeline.unmatched_stops != 1ULL ||
        timeline.missing_call_trace_ids != 1ULL ||
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
        !test_invalid_input()) {
        fputs("COM timeline tests failed.\n", stderr);
        return 1;
    }

    puts("COM timeline tests passed.");
    return 0;
}
