#include "ole_timeline.h"

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static void increment_count(ULONGLONG *value)
{
    if (*value != (ULONGLONG)-1) {
        ++(*value);
    }
}

static int ensure_record_capacity(COMTRACE_OLE_TIMELINE *timeline)
{
    size_t capacity;
    COMTRACE_OLE_TIMELINE_RECORD *records;

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

    records = (COMTRACE_OLE_TIMELINE_RECORD *)realloc(
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

static int ensure_pending_capacity(COMTRACE_OLE_TIMELINE *timeline)
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

static int copy_operation(wchar_t *destination, size_t capacity, const wchar_t *source)
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

static void copy_activity_ids(
    COMTRACE_OLE_TIMELINE_RECORD *record,
    const COMTRACE_ACTIVITY_IDS *activity_ids
)
{
    if (activity_ids == NULL) {
        return;
    }

    if (activity_ids->has_activity_id) {
        record->activity_id = activity_ids->activity_id;
        record->has_activity_id = TRUE;
    }

    if (activity_ids->has_related_activity_id) {
        record->related_activity_id = activity_ids->related_activity_id;
        record->has_related_activity_id = TRUE;
    }
}

static COMTRACE_OLE_TIMELINE_MATCH_TYPE get_match_type(
    const COMTRACE_OLE_TIMELINE_RECORD *record,
    const COMTRACE_OLE_TIMELINE_EVENT *event
)
{
    BOOL event_has_activity = event->activity_ids != NULL &&
                              event->activity_ids->has_activity_id;
    BOOL event_has_related_activity = event->activity_ids != NULL &&
                                      event->activity_ids->has_related_activity_id;

    if (wcscmp(record->operation, event->operation) != 0) {
        return COMTRACE_OLE_TIMELINE_MATCH_NONE;
    }

    if (record->has_activity_id || event_has_activity) {
        if (record->has_activity_id &&
            event_has_activity &&
            memcmp(
                &record->activity_id,
                &event->activity_ids->activity_id,
                sizeof(GUID)
            ) == 0) {
            return COMTRACE_OLE_TIMELINE_MATCH_ACTIVITY_ID;
        }
        return COMTRACE_OLE_TIMELINE_MATCH_NONE;
    }

    if (record->has_related_activity_id || event_has_related_activity) {
        if (record->has_related_activity_id &&
            event_has_related_activity &&
            memcmp(
                &record->related_activity_id,
                &event->activity_ids->related_activity_id,
                sizeof(GUID)
            ) == 0) {
            return COMTRACE_OLE_TIMELINE_MATCH_RELATED_ACTIVITY_ID;
        }
        return COMTRACE_OLE_TIMELINE_MATCH_NONE;
    }

    if (record->process_id == event->process_id &&
        record->thread_id == event->thread_id) {
        return COMTRACE_OLE_TIMELINE_MATCH_PROCESS_THREAD;
    }

    return COMTRACE_OLE_TIMELINE_MATCH_NONE;
}

void comtrace_ole_timeline_init(COMTRACE_OLE_TIMELINE *timeline)
{
    if (timeline != NULL) {
        ZeroMemory(timeline, sizeof(*timeline));
    }
}

void comtrace_ole_timeline_destroy(COMTRACE_OLE_TIMELINE *timeline)
{
    if (timeline == NULL) {
        return;
    }

    free(timeline->records);
    free(timeline->pending_indices);
    ZeroMemory(timeline, sizeof(*timeline));
}

COMTRACE_OLE_TIMELINE_STATUS comtrace_ole_timeline_add_event(
    COMTRACE_OLE_TIMELINE *timeline,
    const COMTRACE_OLE_TIMELINE_EVENT *event
)
{
    size_t position;
    size_t record_index;
    size_t selected_pending_index = (size_t)-1;
    size_t cross_candidate_index = (size_t)-1;
    size_t same_thread_candidate_count = 0U;
    size_t cross_candidate_count = 0U;
    size_t match_count = 0U;
    COMTRACE_OLE_TIMELINE_RECORD *record;
    COMTRACE_OLE_TIMELINE_MATCH_TYPE match_type;
    if (timeline == NULL ||
        event == NULL ||
        event->operation == NULL ||
        (event->phase != COMTRACE_OLE_TIMELINE_PHASE_START &&
         event->phase != COMTRACE_OLE_TIMELINE_PHASE_STOP)) {
        return COMTRACE_OLE_TIMELINE_INVALID_ARGUMENT;
    }

    if (event->phase == COMTRACE_OLE_TIMELINE_PHASE_START) {
        if (!ensure_record_capacity(timeline) ||
            !ensure_pending_capacity(timeline)) {
            return COMTRACE_OLE_TIMELINE_ALLOCATION_FAILED;
        }

        record_index = timeline->record_count;
        record = &timeline->records[record_index];
        ZeroMemory(record, sizeof(*record));

        if (!copy_operation(record->operation, _countof(record->operation), event->operation)) {
            return COMTRACE_OLE_TIMELINE_INVALID_ARGUMENT;
        }

        record->process_id = event->process_id;
        record->thread_id = event->thread_id;
        record->start_timestamp = event->timestamp;
        record->has_start = TRUE;
        copy_activity_ids(record, event->activity_ids);

        ++timeline->record_count;
        if (event->activity_ids == NULL ||
            !event->activity_ids->has_activity_id) {
            increment_count(&timeline->missing_activity_ids);
        }
        timeline->pending_indices[timeline->pending_count] = record_index;
        ++timeline->pending_count;
        return COMTRACE_OLE_TIMELINE_SUCCESS;
    }

    for (position = 0U; position < timeline->pending_count; ++position) {
        record_index = timeline->pending_indices[position];
        record = &timeline->records[record_index];
        if (get_match_type(record, event) == COMTRACE_OLE_TIMELINE_MATCH_NONE) {
            continue;
        }
        if (record->process_id == event->process_id &&
            record->thread_id == event->thread_id) {
            selected_pending_index = position;
            ++same_thread_candidate_count;
        } else {
            if (cross_candidate_count == 0U) {
                cross_candidate_index = position;
            }
            ++cross_candidate_count;
        }
    }

    if (same_thread_candidate_count != 0U) {
        match_count = 1U;
    } else {
        selected_pending_index = cross_candidate_index;
        match_count = cross_candidate_count;
    }

    if (match_count == 1U) {
        size_t pending_index = selected_pending_index;
        record_index = timeline->pending_indices[pending_index];
        record = &timeline->records[record_index];
        match_type = get_match_type(record, event);
        record->match_type = match_type;
        record->has_stop = TRUE;
        record->stop_process_id = event->process_id;
        record->stop_thread_id = event->thread_id;
        record->stop_timestamp = event->timestamp;

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
            record->related_activity_id = event->activity_ids->related_activity_id;
            record->has_related_activity_id = TRUE;
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
        return COMTRACE_OLE_TIMELINE_SUCCESS;
    }

    if (!ensure_record_capacity(timeline)) {
        return COMTRACE_OLE_TIMELINE_ALLOCATION_FAILED;
    }

    record_index = timeline->record_count;
    record = &timeline->records[record_index];
    ZeroMemory(record, sizeof(*record));

    if (!copy_operation(record->operation, _countof(record->operation), event->operation)) {
        return COMTRACE_OLE_TIMELINE_INVALID_ARGUMENT;
    }

    record->process_id = event->process_id;
    record->thread_id = event->thread_id;
    record->stop_process_id = event->process_id;
    record->stop_thread_id = event->thread_id;
    record->stop_timestamp = event->timestamp;
    record->has_stop = TRUE;
    record->ambiguous_match = match_count > 1U;
    copy_activity_ids(record, event->activity_ids);

    ++timeline->record_count;
    increment_count(&timeline->unmatched_stops);
    if (record->ambiguous_match) {
        increment_count(&timeline->ambiguous_stops);
    }
    if (event->activity_ids == NULL ||
        !event->activity_ids->has_activity_id) {
        ++timeline->missing_activity_ids;
    }
    return COMTRACE_OLE_TIMELINE_SUCCESS;
}

size_t comtrace_ole_timeline_count(const COMTRACE_OLE_TIMELINE *timeline)
{
    return timeline == NULL ? 0U : timeline->record_count;
}

size_t comtrace_ole_timeline_pending_count(const COMTRACE_OLE_TIMELINE *timeline)
{
    return timeline == NULL ? 0U : timeline->pending_count;
}

size_t comtrace_ole_timeline_sequence_count(const COMTRACE_OLE_TIMELINE *timeline)
{
    size_t count = 0U;
    size_t index;

    if (timeline == NULL) {
        return 0U;
    }

    for (index = 0U; index < timeline->record_count; ++index) {
        size_t previous;
        int found = 0;

        if (!timeline->records[index].has_activity_id) {
            continue;
        }

        for (previous = 0U; previous < index; ++previous) {
            if (timeline->records[previous].has_activity_id &&
                memcmp(
                    &timeline->records[previous].activity_id,
                    &timeline->records[index].activity_id,
                    sizeof(GUID)
                ) == 0) {
                found = 1;
                break;
            }
        }

        if (!found) {
            ++count;
        }
    }

    return count;
}

const COMTRACE_OLE_TIMELINE_RECORD *comtrace_ole_timeline_get(
    const COMTRACE_OLE_TIMELINE *timeline,
    size_t index
)
{
    if (timeline == NULL || index >= timeline->record_count) {
        return NULL;
    }

    return &timeline->records[index];
}
