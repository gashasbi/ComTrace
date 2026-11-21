#include <stdio.h>
#include <string.h>
#include "ole_timeline.h"

static const GUID activity_one = {
    0x10213243, 0x5465, 0x7687,
    { 0x98, 0xa9, 0xba, 0xcb, 0xdc, 0xed, 0xfe, 0x0f }
};

static const GUID activity_two = {
    0x20314253, 0x6475, 0x8697,
    { 0xa8, 0xb9, 0xca, 0xdb, 0xec, 0xfd, 0x0e, 0x1f }
};

static int test_drag_drop_operation_pair(void)
{
    COMTRACE_OLE_TIMELINE timeline;
    COMTRACE_OLE_TIMELINE_EVENT event;
    COMTRACE_ACTIVITY_IDS activity_ids;
    const COMTRACE_OLE_TIMELINE_RECORD *record;

    comtrace_ole_timeline_init(&timeline);
    ZeroMemory(&event, sizeof(event));
    ZeroMemory(&activity_ids, sizeof(activity_ids));

    activity_ids.has_activity_id = TRUE;
    activity_ids.activity_id = activity_one;

    event.operation = L"OLE_DragDrop_DoDragDrop";
    event.phase = COMTRACE_OLE_TIMELINE_PHASE_START;
    event.process_id = 100U;
    event.thread_id = 200U;
    event.timestamp = 1000ULL;
    event.activity_ids = &activity_ids;

    if (comtrace_ole_timeline_add_event(&timeline, &event) !=
        COMTRACE_OLE_TIMELINE_SUCCESS) {
        comtrace_ole_timeline_destroy(&timeline);
        return 0;
    }

    event.phase = COMTRACE_OLE_TIMELINE_PHASE_STOP;
    event.timestamp = 1600ULL;
    activity_ids.has_related_activity_id = TRUE;
    activity_ids.related_activity_id = activity_two;

    if (comtrace_ole_timeline_add_event(&timeline, &event) !=
        COMTRACE_OLE_TIMELINE_SUCCESS) {
        comtrace_ole_timeline_destroy(&timeline);
        return 0;
    }

    record = comtrace_ole_timeline_get(&timeline, 0U);
    if (record == NULL ||
        !record->has_start ||
        !record->has_stop ||
        !record->has_duration ||
        record->duration_ticks != 600ULL ||
        record->match_type != COMTRACE_OLE_TIMELINE_MATCH_ACTIVITY_ID ||
        !record->has_activity_id ||
        !record->has_related_activity_id ||
        memcmp(&record->activity_id, &activity_one, sizeof(GUID)) != 0 ||
        memcmp(&record->related_activity_id, &activity_two, sizeof(GUID)) != 0 ||
        comtrace_ole_timeline_sequence_count(&timeline) != 1U ||
        comtrace_ole_timeline_pending_count(&timeline) != 0U) {
        comtrace_ole_timeline_destroy(&timeline);
        return 0;
    }

    comtrace_ole_timeline_destroy(&timeline);
    return 1;
}

static int test_activity_mismatch_does_not_pair(void)
{
    COMTRACE_OLE_TIMELINE timeline;
    COMTRACE_OLE_TIMELINE_EVENT event;
    COMTRACE_ACTIVITY_IDS first_activity;
    COMTRACE_ACTIVITY_IDS second_activity;

    comtrace_ole_timeline_init(&timeline);
    ZeroMemory(&event, sizeof(event));
    ZeroMemory(&first_activity, sizeof(first_activity));
    ZeroMemory(&second_activity, sizeof(second_activity));

    first_activity.has_activity_id = TRUE;
    first_activity.activity_id = activity_one;
    second_activity.has_activity_id = TRUE;
    second_activity.activity_id = activity_two;

    event.operation = L"OLE_Clipboard_SetClipboard";
    event.phase = COMTRACE_OLE_TIMELINE_PHASE_START;
    event.process_id = 50U;
    event.thread_id = 60U;
    event.timestamp = 100ULL;
    event.activity_ids = &first_activity;

    if (comtrace_ole_timeline_add_event(&timeline, &event) !=
        COMTRACE_OLE_TIMELINE_SUCCESS) {
        comtrace_ole_timeline_destroy(&timeline);
        return 0;
    }

    event.phase = COMTRACE_OLE_TIMELINE_PHASE_STOP;
    event.timestamp = 200ULL;
    event.activity_ids = &second_activity;

    if (comtrace_ole_timeline_add_event(&timeline, &event) !=
        COMTRACE_OLE_TIMELINE_SUCCESS ||
        comtrace_ole_timeline_pending_count(&timeline) != 1U ||
        comtrace_ole_timeline_count(&timeline) != 2U ||
        timeline.unmatched_stops != 1ULL ||
        comtrace_ole_timeline_sequence_count(&timeline) != 2U) {
        comtrace_ole_timeline_destroy(&timeline);
        return 0;
    }

    comtrace_ole_timeline_destroy(&timeline);
    return 1;
}

static int test_missing_activity_and_invalid_input(void)
{
    COMTRACE_OLE_TIMELINE timeline;
    COMTRACE_OLE_TIMELINE_EVENT event;

    comtrace_ole_timeline_init(&timeline);
    ZeroMemory(&event, sizeof(event));

    if (comtrace_ole_timeline_add_event(&timeline, NULL) !=
            COMTRACE_OLE_TIMELINE_INVALID_ARGUMENT ||
        comtrace_ole_timeline_add_event(&timeline, &event) !=
            COMTRACE_OLE_TIMELINE_INVALID_ARGUMENT) {
        comtrace_ole_timeline_destroy(&timeline);
        return 0;
    }

    event.operation = L"OLE_DragDrop_Register";
    event.phase = COMTRACE_OLE_TIMELINE_PHASE_START;
    event.process_id = 1U;
    event.thread_id = 2U;
    event.timestamp = 10ULL;

    if (comtrace_ole_timeline_add_event(&timeline, &event) !=
            COMTRACE_OLE_TIMELINE_SUCCESS ||
        timeline.missing_activity_ids != 1ULL ||
        comtrace_ole_timeline_pending_count(&timeline) != 1U) {
        comtrace_ole_timeline_destroy(&timeline);
        return 0;
    }

    event.phase = COMTRACE_OLE_TIMELINE_PHASE_STOP;
    event.timestamp = 20ULL;

    if (comtrace_ole_timeline_add_event(&timeline, &event) !=
            COMTRACE_OLE_TIMELINE_SUCCESS ||
        comtrace_ole_timeline_pending_count(&timeline) != 0U ||
        comtrace_ole_timeline_sequence_count(&timeline) != 0U ||
        comtrace_ole_timeline_count(&timeline) != 1U ||
        comtrace_ole_timeline_get(&timeline, 0U)->match_type !=
            COMTRACE_OLE_TIMELINE_MATCH_PROCESS_THREAD) {
        comtrace_ole_timeline_destroy(&timeline);
        return 0;
    }

    comtrace_ole_timeline_destroy(&timeline);
    return 1;
}

int main(void)
{
    if (!test_drag_drop_operation_pair() ||
        !test_activity_mismatch_does_not_pair() ||
        !test_missing_activity_and_invalid_input()) {
        fputs("OLE timeline tests failed.\n", stderr);
        return 1;
    }

    puts("OLE timeline tests passed.");
    return 0;
}
