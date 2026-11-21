#include "com_timeline.h"

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static int ensure_record_capacity(COMTRACE_TIMELINE *timeline)
{
    size_t capacity;
    COMTRACE_TIMELINE_RECORD *records;

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

    records = (COMTRACE_TIMELINE_RECORD *)realloc(
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

static int ensure_pending_capacity(COMTRACE_TIMELINE *timeline)
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
    size_t destination_count,
    const wchar_t *operation
)
{
    size_t length;

    if (operation == NULL || operation[0] == L'\0') {
        return 0;
    }

    length = wcslen(operation);
    if (length >= destination_count) {
        return 0;
    }

    CopyMemory(destination, operation, (length + 1U) * sizeof(wchar_t));
    return 1;
}

static int same_key(
    const COMTRACE_TIMELINE_RECORD *record,
    const COMTRACE_TIMELINE_EVENT *event
)
{
    return record->has_call_trace_id &&
           record->call_trace_id == event->call_trace_id &&
           record->process_id == event->process_id &&
           record->thread_id == event->thread_id &&
           wcscmp(record->operation, event->operation) == 0;
}

static void copy_event_data(
    COMTRACE_TIMELINE_RECORD *record,
    const COMTRACE_TIMELINE_EVENT *event
)
{
    record->has_call_trace_id = event->has_call_trace_id;
    record->call_trace_id = event->call_trace_id;
    record->process_id = event->process_id;
    record->thread_id = event->thread_id;

    if (event->object_identity != NULL) {
        CopyMemory(
            &record->object_identity,
            event->object_identity,
            sizeof(record->object_identity)
        );
        record->has_object_identity = TRUE;
    }

    if (event->activity_ids != NULL) {
        CopyMemory(
            &record->activity_ids,
            event->activity_ids,
            sizeof(record->activity_ids)
        );
        record->has_activity_ids = TRUE;
    }
}

void comtrace_timeline_init(COMTRACE_TIMELINE *timeline)
{
    if (timeline != NULL) {
        ZeroMemory(timeline, sizeof(*timeline));
    }
}

void comtrace_timeline_destroy(COMTRACE_TIMELINE *timeline)
{
    if (timeline == NULL) {
        return;
    }

    free(timeline->records);
    free(timeline->pending_indices);
    ZeroMemory(timeline, sizeof(*timeline));
}

COMTRACE_TIMELINE_STATUS comtrace_timeline_add_event(
    COMTRACE_TIMELINE *timeline,
    const COMTRACE_TIMELINE_EVENT *event
)
{
    size_t pending_position;
    size_t record_index;
    COMTRACE_TIMELINE_RECORD *record;

    if (timeline == NULL ||
        event == NULL ||
        event->operation == NULL ||
        (event->phase != COMTRACE_TIMELINE_PHASE_START &&
         event->phase != COMTRACE_TIMELINE_PHASE_STOP)) {
        return COMTRACE_TIMELINE_INVALID_ARGUMENT;
    }

    if (event->phase == COMTRACE_TIMELINE_PHASE_START) {
        if (!ensure_record_capacity(timeline)) {
            return COMTRACE_TIMELINE_ALLOCATION_FAILED;
        }

        if (event->has_call_trace_id && !ensure_pending_capacity(timeline)) {
            return COMTRACE_TIMELINE_ALLOCATION_FAILED;
        }

        record_index = timeline->record_count;
        record = &timeline->records[record_index];
        ZeroMemory(record, sizeof(*record));

        if (!copy_operation(
                record->operation,
                _countof(record->operation),
                event->operation)) {
            return COMTRACE_TIMELINE_INVALID_ARGUMENT;
        }

        copy_event_data(record, event);
        record->has_start = TRUE;
        record->start_timestamp = event->timestamp;
        ++timeline->record_count;

        if (event->has_call_trace_id) {
            timeline->pending_indices[timeline->pending_count] = record_index;
            ++timeline->pending_count;
        } else {
            ++timeline->missing_call_trace_ids;
        }

        return COMTRACE_TIMELINE_SUCCESS;
    }

    if (!event->has_call_trace_id) {
        if (!ensure_record_capacity(timeline)) {
            return COMTRACE_TIMELINE_ALLOCATION_FAILED;
        }

        record_index = timeline->record_count;
        record = &timeline->records[record_index];
        ZeroMemory(record, sizeof(*record));

        if (!copy_operation(
                record->operation,
                _countof(record->operation),
                event->operation)) {
            return COMTRACE_TIMELINE_INVALID_ARGUMENT;
        }

        copy_event_data(record, event);
        record->has_stop = TRUE;
        record->stop_timestamp = event->timestamp;
        record->has_hresult = event->has_hresult;
        record->hresult = event->hresult;
        ++timeline->record_count;
        ++timeline->missing_call_trace_ids;
        ++timeline->unmatched_stops;
        return COMTRACE_TIMELINE_SUCCESS;
    }

    for (pending_position = timeline->pending_count;
         pending_position > 0U;
         --pending_position) {
        size_t index = pending_position - 1U;
        record_index = timeline->pending_indices[index];
        record = &timeline->records[record_index];

        if (!same_key(record, event)) {
            continue;
        }

        record->has_stop = TRUE;
        record->stop_timestamp = event->timestamp;
        record->has_hresult = event->has_hresult;
        record->hresult = event->hresult;

        if (event->timestamp >= record->start_timestamp) {
            record->duration_ticks = event->timestamp - record->start_timestamp;
            record->has_duration = TRUE;
        }

        if (!record->has_object_identity && event->object_identity != NULL) {
            CopyMemory(
                &record->object_identity,
                event->object_identity,
                sizeof(record->object_identity)
            );
            record->has_object_identity = TRUE;
        }

        if (!record->has_activity_ids && event->activity_ids != NULL) {
            CopyMemory(
                &record->activity_ids,
                event->activity_ids,
                sizeof(record->activity_ids)
            );
            record->has_activity_ids = TRUE;
        }

        if (index + 1U < timeline->pending_count) {
            memmove(
                &timeline->pending_indices[index],
                &timeline->pending_indices[index + 1U],
                (timeline->pending_count - index - 1U) *
                    sizeof(timeline->pending_indices[0])
            );
        }
        --timeline->pending_count;
        return COMTRACE_TIMELINE_SUCCESS;
    }

    if (!ensure_record_capacity(timeline)) {
        return COMTRACE_TIMELINE_ALLOCATION_FAILED;
    }

    record_index = timeline->record_count;
    record = &timeline->records[record_index];
    ZeroMemory(record, sizeof(*record));

    if (!copy_operation(
            record->operation,
            _countof(record->operation),
            event->operation)) {
        return COMTRACE_TIMELINE_INVALID_ARGUMENT;
    }

    copy_event_data(record, event);
    record->has_stop = TRUE;
    record->stop_timestamp = event->timestamp;
    record->has_hresult = event->has_hresult;
    record->hresult = event->hresult;
    ++timeline->record_count;
    ++timeline->unmatched_stops;

    return COMTRACE_TIMELINE_SUCCESS;
}

size_t comtrace_timeline_count(const COMTRACE_TIMELINE *timeline)
{
    return timeline == NULL ? 0U : timeline->record_count;
}

size_t comtrace_timeline_pending_count(const COMTRACE_TIMELINE *timeline)
{
    return timeline == NULL ? 0U : timeline->pending_count;
}

const COMTRACE_TIMELINE_RECORD *comtrace_timeline_get(
    const COMTRACE_TIMELINE *timeline,
    size_t index
)
{
    if (timeline == NULL || index >= timeline->record_count) {
        return NULL;
    }

    return &timeline->records[index];
}
