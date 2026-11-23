#include "com_lifetime_analysis.h"

#include <stdlib.h>
#include <string.h>

static void increment_count(ULONGLONG *value)
{
    if (*value != (ULONGLONG)-1) {
        ++(*value);
    }
}

static int ensure_array_capacity(
    void **items,
    size_t *capacity,
    size_t required,
    size_t item_size
)
{
    size_t next_capacity;
    void *resized;

    if (*capacity >= required) {
        return 1;
    }

    next_capacity = *capacity == 0U ? 4U : *capacity;
    while (next_capacity < required) {
        if (next_capacity > (size_t)-1 / 2U) {
            next_capacity = required;
            break;
        }
        next_capacity *= 2U;
    }

    if (item_size == 0U || next_capacity > (size_t)-1 / item_size) {
        return 0;
    }

    resized = realloc(*items, next_capacity * item_size);
    if (resized == NULL) {
        return 0;
    }

    *items = resized;
    *capacity = next_capacity;
    return 1;
}

static int add_interface_id(
    COMTRACE_LIFETIME_RECORD *record,
    const GUID *interface_id
)
{
    size_t index;

    for (index = 0U; index < record->interface_id_count; ++index) {
        if (memcmp(&record->interface_ids[index].interface_id,
                   interface_id, sizeof(GUID)) == 0) {
            return 1;
        }
    }

    if (!ensure_array_capacity(
            (void **)&record->interface_ids,
            &record->interface_id_capacity,
            record->interface_id_count + 1U,
            sizeof(*record->interface_ids))) {
        return 0;
    }

    record->interface_ids[record->interface_id_count].interface_id = *interface_id;
    ++record->interface_id_count;

    if (!record->has_interface_id) {
        record->has_interface_id = TRUE;
        record->interface_id = *interface_id;
    }

    return 1;
}

static int add_native_identity(
    COMTRACE_LIFETIME_RECORD *record,
    const COMTRACE_LIFETIME_EVENT *event
)
{
    size_t index;
    COMTRACE_LIFETIME_NATIVE_IDENTITY *identity;

    for (index = 0U; index < record->native_identity_count; ++index) {
        identity = &record->native_identities[index];
        if (identity->native_object_id == event->native_object_id &&
            identity->has_native_ipid == event->has_native_ipid &&
            (!event->has_native_ipid ||
             memcmp(&identity->native_ipid, &event->native_ipid, sizeof(GUID)) == 0) &&
            identity->has_interface_id == event->has_interface_id &&
            (!event->has_interface_id ||
             memcmp(&identity->interface_id, &event->interface_id, sizeof(GUID)) == 0)) {
            return 1;
        }
    }

    if (!ensure_array_capacity(
            (void **)&record->native_identities,
            &record->native_identity_capacity,
            record->native_identity_count + 1U,
            sizeof(*record->native_identities))) {
        return 0;
    }

    identity = &record->native_identities[record->native_identity_count];
    ZeroMemory(identity, sizeof(*identity));
    identity->native_object_id = event->native_object_id;
    identity->has_native_ipid = event->has_native_ipid;
    identity->native_ipid = event->native_ipid;
    identity->has_interface_id = event->has_interface_id;
    identity->interface_id = event->interface_id;
    ++record->native_identity_count;

    if (!record->has_native_object_id) {
        record->has_native_object_id = TRUE;
        record->native_object_id = event->native_object_id;
    }
    if (event->has_native_ipid && !record->has_native_ipid) {
        record->has_native_ipid = TRUE;
        record->native_ipid = event->native_ipid;
    }

    return 1;
}

static int add_activity_id(
    COMTRACE_LIFETIME_RECORD *record,
    const COMTRACE_LIFETIME_EVENT *event
)
{
    size_t index;
    COMTRACE_LIFETIME_ACTIVITY_ID *activity;

    for (index = 0U; index < record->activity_id_count; ++index) {
        activity = &record->activity_ids[index];
        if (activity->has_activity_id == event->has_activity_id &&
            (!event->has_activity_id ||
             memcmp(&activity->activity_id, &event->activity_id, sizeof(GUID)) == 0) &&
            activity->has_related_activity_id == event->has_related_activity_id &&
            (!event->has_related_activity_id ||
             memcmp(&activity->related_activity_id, &event->related_activity_id, sizeof(GUID)) == 0)) {
            return 1;
        }
    }

    if (!ensure_array_capacity(
            (void **)&record->activity_ids,
            &record->activity_id_capacity,
            record->activity_id_count + 1U,
            sizeof(*record->activity_ids))) {
        return 0;
    }

    activity = &record->activity_ids[record->activity_id_count];
    ZeroMemory(activity, sizeof(*activity));
    activity->has_activity_id = event->has_activity_id;
    activity->activity_id = event->activity_id;
    activity->has_related_activity_id = event->has_related_activity_id;
    activity->related_activity_id = event->related_activity_id;
    ++record->activity_id_count;
    return 1;
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
        record->has_instrumentation_id == event->has_instrumentation_id &&
        (!event->has_instrumentation_id ||
         memcmp(&record->instrumentation_id, &event->instrumentation_id, sizeof(GUID)) == 0) &&
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
    record->has_instrumentation_id = event->has_instrumentation_id;
    record->instrumentation_id = event->instrumentation_id;
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

    {
        size_t index;
        for (index = 0U; index < analysis->record_count; ++index) {
            free(analysis->records[index].interface_ids);
            free(analysis->records[index].native_identities);
            free(analysis->records[index].activity_ids);
        }
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
        event->type > COMTRACE_LIFETIME_EVENT_NATIVE_IDENTITY_BOUND ||
        (event->type == COMTRACE_LIFETIME_EVENT_NATIVE_IDENTITY_BOUND &&
         (!event->has_native_object_id || event->native_object_id == 0ULL ||
          !event->has_interface_id))) {
        return COMTRACE_LIFETIME_INVALID_ARGUMENT;
    }

    record = find_record(analysis, event);
    if (record == NULL) {
        record = create_record(analysis, event);
        if (record == NULL) {
            return COMTRACE_LIFETIME_ALLOCATION_FAILED;
        }
    }

    if (event->has_interface_id &&
        !add_interface_id(record, &event->interface_id)) {
        return COMTRACE_LIFETIME_ALLOCATION_FAILED;
    }
    if (event->type == COMTRACE_LIFETIME_EVENT_NATIVE_IDENTITY_BOUND &&
        !add_native_identity(record, event)) {
        return COMTRACE_LIFETIME_ALLOCATION_FAILED;
    }
    if ((event->has_activity_id || event->has_related_activity_id) &&
        !add_activity_id(record, event)) {
        return COMTRACE_LIFETIME_ALLOCATION_FAILED;
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
    case COMTRACE_LIFETIME_EVENT_NATIVE_IDENTITY_BOUND:
        increment_count(&analysis->native_identity_bind_events);
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
