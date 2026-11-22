#include <stdio.h>
#include <string.h>
#include "com_lifetime_analysis.h"

static const GUID class_id = {
    0x183a5c71,
    0x2f49,
    0x4d83,
    {0x91, 0x06, 0x27, 0x4a, 0x5b, 0x6c, 0x7d, 0x8e}
};

static const GUID interface_id = {
    0x296b7d82,
    0x3a5c,
    0x4e94,
    {0xa2, 0x17, 0x38, 0x5b, 0x6c, 0x7d, 0x8e, 0x9f}
};

static int add_event(
    COMTRACE_COM_LIFETIME_ANALYSIS *analysis,
    COMTRACE_LIFETIME_EVENT_TYPE type,
    DWORD process_id,
    ULONGLONG generation,
    BOOL has_generation,
    ULONGLONG object_id,
    ULONGLONG timestamp,
    BOOL has_count,
    ULONG count,
    const COMTRACE_LIFETIME_RECORD **record
)
{
    COMTRACE_LIFETIME_EVENT event;

    ZeroMemory(&event, sizeof(event));
    event.type = type;
    event.process_id = process_id;
    event.object_instance_id = object_id;
    event.process_generation = generation;
    event.has_process_generation = has_generation;
    event.timestamp = timestamp;
    event.has_interface_id = TRUE;
    event.interface_id = interface_id;
    event.has_reference_count_result = has_count;
    event.reference_count_result = count;

    if (type == COMTRACE_LIFETIME_EVENT_CREATED) {
        event.has_class_id = TRUE;
        event.class_id = class_id;
        event.has_initial_reference_count = TRUE;
        event.initial_reference_count = count;
    }

    return comtrace_lifetime_analysis_add_event(analysis, &event, record) ==
        COMTRACE_LIFETIME_SUCCESS;
}

static int test_lifetime_sequence(void)
{
    COMTRACE_COM_LIFETIME_ANALYSIS analysis;
    const COMTRACE_LIFETIME_RECORD *record = NULL;

    comtrace_lifetime_analysis_init(&analysis);

    if (!add_event(&analysis, COMTRACE_LIFETIME_EVENT_CREATED, 100U, 1ULL, TRUE, 7ULL, 10ULL, TRUE, 1U, &record) ||
        !add_event(&analysis, COMTRACE_LIFETIME_EVENT_ADD_REF, 100U, 1ULL, TRUE, 7ULL, 20ULL, TRUE, 2U, &record) ||
        !add_event(&analysis, COMTRACE_LIFETIME_EVENT_RELEASE, 100U, 1ULL, TRUE, 7ULL, 30ULL, TRUE, 1U, &record) ||
        !add_event(&analysis, COMTRACE_LIFETIME_EVENT_RELEASE, 100U, 1ULL, TRUE, 7ULL, 40ULL, TRUE, 0U, &record)) {
        comtrace_lifetime_analysis_destroy(&analysis);
        return 0;
    }

    record = comtrace_lifetime_analysis_get(&analysis, 0U);
    if (record == NULL ||
        !record->has_created ||
        record->has_destroyed ||
        record->create_events != 1ULL ||
        record->add_ref_events != 1ULL ||
        record->release_events != 2ULL ||
        record->release_zero_results != 1ULL ||
        !record->has_last_reference_count_result ||
        record->last_reference_count_result != 0U ||
        record->has_destroyed ||
        !record->has_class_id ||
        memcmp(&record->class_id, &class_id, sizeof(GUID)) != 0) {
        comtrace_lifetime_analysis_destroy(&analysis);
        return 0;
    }

    if (!add_event(&analysis, COMTRACE_LIFETIME_EVENT_DESTROYED, 100U, 1ULL, TRUE, 7ULL, 50ULL, FALSE, 0U, &record)) {
        comtrace_lifetime_analysis_destroy(&analysis);
        return 0;
    }

    record = comtrace_lifetime_analysis_get(&analysis, 0U);
    if (record == NULL ||
        !record->has_destroyed ||
        record->destroyed_timestamp != 50ULL ||
        record->created_timestamp != 10ULL ||
        record->last_seen_timestamp != 50ULL ||
        record->observation_count != 5ULL ||
        analysis.events_seen != 5ULL) {
        comtrace_lifetime_analysis_destroy(&analysis);
        return 0;
    }

    comtrace_lifetime_analysis_destroy(&analysis);
    return 1;
}

static int test_process_identity_separation(void)
{
    COMTRACE_COM_LIFETIME_ANALYSIS analysis;
    const COMTRACE_LIFETIME_RECORD *record = NULL;

    comtrace_lifetime_analysis_init(&analysis);

    if (!add_event(&analysis, COMTRACE_LIFETIME_EVENT_CREATED, 100U, 1ULL, TRUE, 9ULL, 10ULL, TRUE, 1U, &record) ||
        !add_event(&analysis, COMTRACE_LIFETIME_EVENT_CREATED, 100U, 2ULL, TRUE, 9ULL, 20ULL, TRUE, 1U, &record) ||
        !add_event(&analysis, COMTRACE_LIFETIME_EVENT_CREATED, 200U, 1ULL, TRUE, 9ULL, 30ULL, TRUE, 1U, &record) ||
        comtrace_lifetime_analysis_count(&analysis) != 3U) {
        comtrace_lifetime_analysis_destroy(&analysis);
        return 0;
    }

    record = comtrace_lifetime_analysis_get(&analysis, 0U);
    if (record == NULL || record->process_id != 100U || record->process_generation != 1ULL) {
        comtrace_lifetime_analysis_destroy(&analysis);
        return 0;
    }

    record = comtrace_lifetime_analysis_get(&analysis, 1U);
    if (record == NULL || record->process_id != 100U || record->process_generation != 2ULL) {
        comtrace_lifetime_analysis_destroy(&analysis);
        return 0;
    }

    record = comtrace_lifetime_analysis_get(&analysis, 2U);
    if (record == NULL || record->process_id != 200U) {
        comtrace_lifetime_analysis_destroy(&analysis);
        return 0;
    }

    comtrace_lifetime_analysis_destroy(&analysis);
    return 1;
}

static int test_lifetime_anomalies(void)
{
    COMTRACE_COM_LIFETIME_ANALYSIS analysis;
    const COMTRACE_LIFETIME_RECORD *record = NULL;

    comtrace_lifetime_analysis_init(&analysis);

    if (!add_event(&analysis, COMTRACE_LIFETIME_EVENT_RELEASE, 100U, 0ULL, FALSE, 5ULL, 10ULL, TRUE, 0U, &record) ||
        !add_event(&analysis, COMTRACE_LIFETIME_EVENT_DESTROYED, 100U, 0ULL, FALSE, 5ULL, 20ULL, FALSE, 0U, &record) ||
        !add_event(&analysis, COMTRACE_LIFETIME_EVENT_CREATED, 100U, 0ULL, FALSE, 5ULL, 30ULL, TRUE, 1U, &record) ||
        !add_event(&analysis, COMTRACE_LIFETIME_EVENT_DESTROYED, 100U, 0ULL, FALSE, 5ULL, 40ULL, FALSE, 0U, &record) ||
        !add_event(&analysis, COMTRACE_LIFETIME_EVENT_DESTROYED, 100U, 0ULL, FALSE, 5ULL, 50ULL, FALSE, 0U, &record) ||
        !add_event(&analysis, COMTRACE_LIFETIME_EVENT_ADD_REF, 100U, 0ULL, FALSE, 5ULL, 60ULL, TRUE, 2U, &record)) {
        comtrace_lifetime_analysis_destroy(&analysis);
        return 0;
    }

    if (analysis.events_without_creation != 2ULL ||
        analysis.events_after_destruction != 4ULL ||
        analysis.destructions_without_creation != 1ULL ||
        analysis.duplicate_destructions != 2ULL ||
        analysis.duplicate_creations != 0ULL ||
        analysis.record_count != 1U) {
        comtrace_lifetime_analysis_destroy(&analysis);
        return 0;
    }

    if (comtrace_lifetime_analysis_add_event(&analysis, NULL, NULL) !=
            COMTRACE_LIFETIME_INVALID_ARGUMENT) {
        comtrace_lifetime_analysis_destroy(&analysis);
        return 0;
    }

    comtrace_lifetime_analysis_destroy(&analysis);
    return 1;
}

int main(void)
{
    if (!test_lifetime_sequence() ||
        !test_process_identity_separation() ||
        !test_lifetime_anomalies()) {
        fputs("COM lifetime analysis tests failed.\n", stderr);
        return 1;
    }

    puts("COM lifetime analysis tests passed.");
    return 0;
}
