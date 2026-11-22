#include "com_lifetime_analysis.h"

#include <stdlib.h>
#include <string.h>

static void increment_count(ULONGLONG *value)
{
    if (*value != (ULONGLONG)-1) {
        ++(*value);
    }
}

static int ensure_record_capacity(COMTRACE_COM_LIFETIME_ANALYSIS *analysis)
{
    size_t capacity;
    COMTRACE_LIFETIME_RECORD *records;

    if (analysis->record_count < analysis->record_capacity) {
        return 1;
    }

    capacity = analysis->record_capacity == 0U
        ? 16U
        : analysis->record_capacity * 2U;

    if (capacity < analysis->record_capacity ||
        capacity > (size_t)-1 / sizeof(*records)) {
        return 0;
    }

    records = (COMTRACE_LIFETIME_RECORD *)realloc(
        analysis->records,
        capacity * sizeof(*records)
    );
    if (records == NULL) {
        return 0;
    }

    analysis->records = records;
    analysis->record_capacity = capacity;
    return 1;
}

static int record_matches_event(
    const COMTRACE_LIFETIME_RECORD *record,
    const COMTRACE_LIFETIME_EVENT *event
)
{
    return record->process_id == event->process_id &&
        record->object_instance_id == event->object_instance_id &&
        record->has_process_generation == event->has_process_generation &&
        (!event->has_process_generation ||
         record->process_generation == event->process_generation);
}

static COMTRACE_LIFETIME_RECORD *find_record(
    COMTRACE_COM_LIFETIME_ANALYSIS *analysis,
    const COMTRACE_LIFETIME_EVENT *event
)
{
    size_t index;

    for (index = 0U; index < analysis->record_count; ++index) {
        if (record_matches_event(&analysis->records[index], event)) {
            return &analysis->records[index];
        }
    }

    return NULL;
}

static COMTRACE_LIFETIME_RECORD *create_record(
    COMTRACE_COM_LIFETIME_ANALYSIS *analysis,
    const COMTRACE_LIFETIME_EVENT *event
)
{
    COMTRACE_LIFETIME_RECORD *record;

    if (!ensure_record_capacity(analysis)) {
        return NULL;
    }

    record = &analysis->records[analysis->record_count];
    ZeroMemory(record, sizeof(*record));
    record->process_id = event->process_id;
    record->object_instance_id = event->object_instance_id;
    record->process_generation = event->process_generation;
    record->has_process_generation = event->has_process_generation;
    record->first_seen_timestamp = event->timestamp;
    record->last_seen_timestamp = event->timestamp;
    ++analysis->record_count;
    return record;
}

void comtrace_lifetime_analysis_init(
    COMTRACE_COM_LIFETIME_ANALYSIS *analysis
)
{
    if (analysis != NULL) {
        ZeroMemory(analysis, sizeof(*analysis));
    }
}

void comtrace_lifetime_analysis_destroy(
    COMTRACE_COM_LIFETIME_ANALYSIS *analysis
)
{
    if (analysis == NULL) {
        return;
    }

    free(analysis->records);
    ZeroMemory(analysis, sizeof(*analysis));
}

COMTRACE_LIFETIME_STATUS comtrace_lifetime_analysis_add_event(
    COMTRACE_COM_LIFETIME_ANALYSIS *analysis,
    const COMTRACE_LIFETIME_EVENT *event,
    const COMTRACE_LIFETIME_RECORD **record_out
)
{
    COMTRACE_LIFETIME_RECORD *record;

    if (record_out != NULL) {
        *record_out = NULL;
    }

    if (analysis == NULL ||
        event == NULL ||
        event->process_id == 0U ||
        event->object_instance_id == 0ULL ||
        event->type < COMTRACE_LIFETIME_EVENT_CREATED ||
        event->type > COMTRACE_LIFETIME_EVENT_DESTROYED) {
        return COMTRACE_LIFETIME_INVALID_ARGUMENT;
    }

    record = find_record(analysis, event);
    if (record == NULL) {
        record = create_record(analysis, event);
        if (record == NULL) {
            return COMTRACE_LIFETIME_ALLOCATION_FAILED;
        }
    }

    increment_count(&analysis->events_seen);
    increment_count(&record->observation_count);

    if (event->timestamp < record->first_seen_timestamp) {
        record->first_seen_timestamp = event->timestamp;
    }
    if (event->timestamp > record->last_seen_timestamp) {
        record->last_seen_timestamp = event->timestamp;
    }

    if (record->has_destroyed) {
        increment_count(&analysis->events_after_destruction);
    }

    if (event->type != COMTRACE_LIFETIME_EVENT_CREATED &&
        !record->has_created) {
        increment_count(&analysis->events_without_creation);
    }

    if (!record->has_interface_id && event->has_interface_id) {
        record->interface_id = event->interface_id;
        record->has_interface_id = TRUE;
    }

    switch (event->type) {
    case COMTRACE_LIFETIME_EVENT_CREATED:
        increment_count(&analysis->create_events);
        increment_count(&record->create_events);
        if (record->has_created) {
            increment_count(&analysis->duplicate_creations);
        } else {
            record->has_created = TRUE;
            record->created_timestamp = event->timestamp;
            if (event->has_class_id) {
                record->class_id = event->class_id;
                record->has_class_id = TRUE;
            }
            if (event->has_initial_reference_count) {
                record->initial_reference_count = event->initial_reference_count;
                record->has_initial_reference_count = TRUE;
            }
        }
        break;
    case COMTRACE_LIFETIME_EVENT_ADD_REF:
        increment_count(&analysis->add_ref_events);
        increment_count(&record->add_ref_events);
        if (event->has_reference_count_result) {
            record->last_reference_count_result = event->reference_count_result;
            record->has_last_reference_count_result = TRUE;
        }
        break;
    case COMTRACE_LIFETIME_EVENT_RELEASE:
        increment_count(&analysis->release_events);
        increment_count(&record->release_events);
        if (event->has_reference_count_result) {
            record->last_reference_count_result = event->reference_count_result;
            record->has_last_reference_count_result = TRUE;
            if (event->reference_count_result == 0U) {
                increment_count(&record->release_zero_results);
            }
        }
        break;
    case COMTRACE_LIFETIME_EVENT_DESTROYED:
        increment_count(&analysis->destroy_events);
        increment_count(&record->destroy_events);
        if (!record->has_created) {
            increment_count(&analysis->destructions_without_creation);
        }
        if (record->has_destroyed) {
            increment_count(&analysis->duplicate_destructions);
        } else {
            record->has_destroyed = TRUE;
            record->destroyed_timestamp = event->timestamp;
        }
        break;
    default:
        return COMTRACE_LIFETIME_INVALID_ARGUMENT;
    }

    if (record_out != NULL) {
        *record_out = record;
    }

    return COMTRACE_LIFETIME_SUCCESS;
}

size_t comtrace_lifetime_analysis_count(
    const COMTRACE_COM_LIFETIME_ANALYSIS *analysis
)
{
    return analysis == NULL ? 0U : analysis->record_count;
}

const COMTRACE_LIFETIME_RECORD *comtrace_lifetime_analysis_get(
    const COMTRACE_COM_LIFETIME_ANALYSIS *analysis,
    size_t index
)
{
    if (analysis == NULL || index >= analysis->record_count) {
        return NULL;
    }

    return &analysis->records[index];
}
