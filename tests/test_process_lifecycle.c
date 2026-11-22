#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <evntrace.h>
#include <evntcons.h>
#include <stdio.h>
#include <string.h>

#include "process_lifecycle.h"

static const GUID process_provider_id = {
    0x3d6fa8d0,
    0xfe05,
    0x11d0,
    { 0x9d, 0xda, 0x00, 0xc0, 0x4f, 0xd7, 0xba, 0x7c }
};

static const GUID other_provider_id = {
    0xb8d6861b,
    0xd20f,
    0x4eec,
    { 0xbb, 0xae, 0x87, 0xe0, 0xdd, 0x80, 0x60, 0x2b }
};

static COMTRACE_PROCESS_EVENT make_event(
    DWORD process_id,
    COMTRACE_PROCESS_EVENT_TYPE type,
    LONGLONG timestamp
)
{
    COMTRACE_PROCESS_EVENT event;

    ZeroMemory(&event, sizeof(event));
    event.process_id = process_id;
    event.type = type;
    event.timestamp = timestamp;
    return event;
}

static int test_event_classification(void)
{
    EVENT_RECORD event_record;

    ZeroMemory(&event_record, sizeof(event_record));
    event_record.EventHeader.ProviderId = process_provider_id;
    event_record.EventHeader.EventDescriptor.Opcode = 1U;
    if (!comtrace_process_lifecycle_is_event(&event_record)) {
        return 0;
    }

    event_record.EventHeader.EventDescriptor.Opcode = 2U;
    if (!comtrace_process_lifecycle_is_event(&event_record)) {
        return 0;
    }

    event_record.EventHeader.EventDescriptor.Opcode = 3U;
    if (!comtrace_process_lifecycle_is_event(&event_record)) {
        return 0;
    }

    event_record.EventHeader.EventDescriptor.Opcode = 4U;
    if (!comtrace_process_lifecycle_is_event(&event_record)) {
        return 0;
    }

    event_record.EventHeader.EventDescriptor.Opcode = 39U;
    if (!comtrace_process_lifecycle_is_event(&event_record)) {
        return 0;
    }

    event_record.EventHeader.EventDescriptor.Opcode = 5U;
    if (comtrace_process_lifecycle_is_event(&event_record)) {
        return 0;
    }

    event_record.EventHeader.ProviderId = other_provider_id;
    event_record.EventHeader.EventDescriptor.Opcode = 1U;
    return !comtrace_process_lifecycle_is_event(&event_record);
}

static int test_pid_reuse_creates_generations(void)
{
    COMTRACE_PROCESS_LIFECYCLE_CATALOG catalog;
    COMTRACE_PROCESS_EVENT event;
    const COMTRACE_PROCESS_LIFETIME *first;
    const COMTRACE_PROCESS_LIFETIME *second;

    comtrace_process_lifecycle_catalog_init(&catalog);

    event = make_event(55U, COMTRACE_PROCESS_EVENT_START, 100);
    event.parent_process_id = 10U;
    event.has_parent_process_id = TRUE;
    if (comtrace_process_lifecycle_catalog_add(&catalog, &event) !=
        COMTRACE_PROCESS_LIFECYCLE_SUCCESS) {
        comtrace_process_lifecycle_catalog_destroy(&catalog);
        return 0;
    }

    event = make_event(55U, COMTRACE_PROCESS_EVENT_END, 200);
    if (comtrace_process_lifecycle_catalog_add(&catalog, &event) !=
        COMTRACE_PROCESS_LIFECYCLE_SUCCESS) {
        comtrace_process_lifecycle_catalog_destroy(&catalog);
        return 0;
    }

    event = make_event(55U, COMTRACE_PROCESS_EVENT_START, 300);
    event.parent_process_id = 20U;
    event.has_parent_process_id = TRUE;
    if (comtrace_process_lifecycle_catalog_add(&catalog, &event) !=
        COMTRACE_PROCESS_LIFECYCLE_SUCCESS) {
        comtrace_process_lifecycle_catalog_destroy(&catalog);
        return 0;
    }

    event = make_event(55U, COMTRACE_PROCESS_EVENT_END, 400);
    if (comtrace_process_lifecycle_catalog_add(&catalog, &event) !=
        COMTRACE_PROCESS_LIFECYCLE_SUCCESS ||
        catalog.count != 2U) {
        comtrace_process_lifecycle_catalog_destroy(&catalog);
        return 0;
    }

    first = &catalog.items[0];
    second = &catalog.items[1];

    if (first->process_id != 55U ||
        first->generation != 1ULL ||
        !first->started_during_trace ||
        !first->has_end_timestamp ||
        first->first_seen_timestamp != 100 ||
        first->end_timestamp != 200 ||
        !first->has_parent_process_id ||
        first->parent_process_id != 10U ||
        second->process_id != 55U ||
        second->generation != 2ULL ||
        !second->started_during_trace ||
        !second->has_end_timestamp ||
        second->first_seen_timestamp != 300 ||
        second->end_timestamp != 400 ||
        !second->has_parent_process_id ||
        second->parent_process_id != 20U ||
        catalog.start_events != 2ULL ||
        catalog.end_events != 2ULL) {
        comtrace_process_lifecycle_catalog_destroy(&catalog);
        return 0;
    }

    comtrace_process_lifecycle_catalog_destroy(&catalog);
    return 1;
}

static int test_rundown_is_not_creation_or_exit(void)
{
    COMTRACE_PROCESS_LIFECYCLE_CATALOG catalog;
    COMTRACE_PROCESS_EVENT event;
    const COMTRACE_PROCESS_LIFETIME *lifetime;

    comtrace_process_lifecycle_catalog_init(&catalog);

    event = make_event(70U, COMTRACE_PROCESS_EVENT_DC_START, 1000);
    if (comtrace_process_lifecycle_catalog_add(&catalog, &event) !=
        COMTRACE_PROCESS_LIFECYCLE_SUCCESS) {
        comtrace_process_lifecycle_catalog_destroy(&catalog);
        return 0;
    }

    event = make_event(70U, COMTRACE_PROCESS_EVENT_DC_END, 2000);
    if (comtrace_process_lifecycle_catalog_add(&catalog, &event) !=
        COMTRACE_PROCESS_LIFECYCLE_SUCCESS ||
        catalog.count != 1U) {
        comtrace_process_lifecycle_catalog_destroy(&catalog);
        return 0;
    }

    lifetime = &catalog.items[0];
    if (lifetime->started_during_trace ||
        !lifetime->has_first_seen_timestamp ||
        lifetime->first_seen_timestamp != 1000 ||
        lifetime->has_end_timestamp ||
        !lifetime->has_trace_end_timestamp ||
        lifetime->trace_end_timestamp != 2000 ||
        catalog.dc_start_events != 1ULL ||
        catalog.dc_end_events != 1ULL) {
        comtrace_process_lifecycle_catalog_destroy(&catalog);
        return 0;
    }

    comtrace_process_lifecycle_catalog_destroy(&catalog);
    return 1;
}

static int test_unmatched_end_is_preserved(void)
{
    COMTRACE_PROCESS_LIFECYCLE_CATALOG catalog;
    COMTRACE_PROCESS_EVENT event;
    const COMTRACE_PROCESS_LIFETIME *lifetime;

    comtrace_process_lifecycle_catalog_init(&catalog);
    event = make_event(80U, COMTRACE_PROCESS_EVENT_END, 5000);

    if (comtrace_process_lifecycle_catalog_add(&catalog, &event) !=
            COMTRACE_PROCESS_LIFECYCLE_SUCCESS ||
        catalog.count != 1U) {
        comtrace_process_lifecycle_catalog_destroy(&catalog);
        return 0;
    }

    lifetime = &catalog.items[0];
    if (lifetime->process_id != 80U ||
        lifetime->generation != 1ULL ||
        lifetime->has_first_seen_timestamp ||
        !lifetime->has_end_timestamp ||
        lifetime->end_timestamp != 5000) {
        comtrace_process_lifecycle_catalog_destroy(&catalog);
        return 0;
    }

    comtrace_process_lifecycle_catalog_destroy(&catalog);
    return 1;
}

static int test_duplicate_start_does_not_create_generation(void)
{
    COMTRACE_PROCESS_LIFECYCLE_CATALOG catalog;
    COMTRACE_PROCESS_EVENT event;

    comtrace_process_lifecycle_catalog_init(&catalog);
    event = make_event(90U, COMTRACE_PROCESS_EVENT_START, 6000);

    if (comtrace_process_lifecycle_catalog_add(&catalog, &event) !=
            COMTRACE_PROCESS_LIFECYCLE_SUCCESS ||
        comtrace_process_lifecycle_catalog_add(&catalog, &event) !=
            COMTRACE_PROCESS_LIFECYCLE_SUCCESS ||
        catalog.count != 1U ||
        catalog.start_events != 2ULL ||
        catalog.items[0].generation != 1ULL) {
        comtrace_process_lifecycle_catalog_destroy(&catalog);
        return 0;
    }

    comtrace_process_lifecycle_catalog_destroy(&catalog);
    return 1;
}


static int test_system_idle_process_id_is_valid(void)
{
    COMTRACE_PROCESS_LIFECYCLE_CATALOG catalog;
    COMTRACE_PROCESS_EVENT event;
    const COMTRACE_PROCESS_LIFETIME *lifetime;

    comtrace_process_lifecycle_catalog_init(&catalog);
    event = make_event(0U, COMTRACE_PROCESS_EVENT_DC_START, 7000);
    event.parent_process_id = 0U;
    event.has_parent_process_id = TRUE;

    if (comtrace_process_lifecycle_catalog_add(&catalog, &event) !=
        COMTRACE_PROCESS_LIFECYCLE_SUCCESS) {
        comtrace_process_lifecycle_catalog_destroy(&catalog);
        return 0;
    }

    event = make_event(0U, COMTRACE_PROCESS_EVENT_DC_END, 8000);
    if (comtrace_process_lifecycle_catalog_add(&catalog, &event) !=
        COMTRACE_PROCESS_LIFECYCLE_SUCCESS ||
        catalog.count != 1U) {
        comtrace_process_lifecycle_catalog_destroy(&catalog);
        return 0;
    }

    lifetime = &catalog.items[0];
    if (lifetime->process_id != 0U ||
        !lifetime->has_parent_process_id ||
        lifetime->parent_process_id != 0U ||
        !lifetime->has_first_seen_timestamp ||
        lifetime->first_seen_timestamp != 7000 ||
        !lifetime->has_trace_end_timestamp ||
        lifetime->trace_end_timestamp != 8000) {
        comtrace_process_lifecycle_catalog_destroy(&catalog);
        return 0;
    }

    comtrace_process_lifecycle_catalog_destroy(&catalog);
    return 1;
}

int main(void)
{
    if (!test_event_classification() ||
        !test_pid_reuse_creates_generations() ||
        !test_rundown_is_not_creation_or_exit() ||
        !test_unmatched_end_is_preserved() ||
        !test_duplicate_start_does_not_create_generation() ||
        !test_system_idle_process_id_is_valid()) {
        fputs("Process lifecycle tests failed.\n", stderr);
        return 1;
    }

    puts("Process lifecycle tests passed.");
    return 0;
}
