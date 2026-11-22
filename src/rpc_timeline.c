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

    if (source == NULL || source[0] == L'\\0') {
        return 0;
    }

    length = wcslen(source);
    if (length >= capacity) {
        return 0;
    }

    CopyMemory(destination, source, (length + 1U) * sizeof(wchar_t));
    return 1;
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
}

static COMTRACE_RPC_TIMELINE_MATCH_TYPE get_match_type(
    const COMTRACE_RPC_TIMELINE_RECORD *record,
    const COMTRACE_RPC_TIMELINE_EVENT *event
)
{
    BOOL event_has_activity = event->activity_ids != NULL &&
        event->activity_ids->has_activity_id;

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
            return COMTRACE_RPC_TIMELINE_MATCH_ACTIVITY_ID;
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
        event->operation[0] == L'\\0' ||
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
        if (event->timestamp >= record->start_timestamp) {
            record->duration_ticks = event->timestamp - record->start_timestamp;
            record->has_duration = TRUE;
        }
        if (!record->has_related_activity_id &&
            event->activity_ids != NULL &&
            event->activity_ids->has_related_activity_id) {
            record->related_activity_id =
                event->activity_ids->related_activity_id;
            record->has_related_activity_id = TRUE;
        }
        if (!record->has_interface_uuid || !record->has_procedure_number) {
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
    record->stop_process_id = event->process_id;
    record->stop_thread_id = event->thread_id;
    record->stop_timestamp = event->timestamp;
    record->has_stop = TRUE;
    copy_event_metadata(record, event);
    ++timeline->record_count;
    increment_count(&timeline->unmatched_stops);
    if (event->activity_ids == NULL ||
        !event->activity_ids->has_activity_id) {
        increment_count(&timeline->missing_activity_ids);
    }

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
