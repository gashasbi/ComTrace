#include "event_catalog.h"

#include <stdio.h>
#include <string.h>

static void check_condition(
    int condition,
    const char *name,
    int *failures
)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", name);
        ++(*failures);
    }
}

static int test_catalog_counting(void)
{
    COMTRACE_EVENT_CATALOG catalog;
    EVENT_RECORD event_record;
    COMTRACE_EVENT_CATALOG_STATUS status;
    const COMTRACE_EVENT_KIND *kind;
    int failures = 0;

    comtrace_event_catalog_init(&catalog);
    memset(&event_record, 0, sizeof(event_record));

    event_record.EventHeader.ProviderId.Data1 = 0x12345678UL;

    event_record.EventHeader.EventDescriptor.Id = 42U;
    event_record.EventHeader.EventDescriptor.Version = 2U;
    event_record.EventHeader.EventDescriptor.Channel = 3U;
    event_record.EventHeader.EventDescriptor.Level = 4U;
    event_record.EventHeader.EventDescriptor.Opcode = 1U;
    event_record.EventHeader.EventDescriptor.Task = 5U;
    event_record.EventHeader.EventDescriptor.Keyword = 0x123456789ABCDEF0ULL;

    status = comtrace_event_catalog_add(&catalog, &event_record);

    check_condition(
        status == COMTRACE_EVENT_CATALOG_SUCCESS,
        "First event is added",
        &failures
    );

    status = comtrace_event_catalog_add(&catalog, &event_record);

    check_condition(
        status == COMTRACE_EVENT_CATALOG_SUCCESS,
        "Repeated event is counted",
        &failures
    );

    check_condition(
        comtrace_event_catalog_count(&catalog) == 1U,
        "Repeated event does not create a second kind",
        &failures
    );

    kind = comtrace_event_catalog_get(&catalog, 0U);

    check_condition(
        kind != NULL && kind->count == 2U,
        "Repeated event count is two",
        &failures
    );

    if (kind != NULL) {
        check_condition(
            kind->provider_id.Data1 == 0x12345678UL &&
            kind->descriptor.Id == 42U &&
            kind->descriptor.Version == 2U &&
            kind->descriptor.Channel == 3U &&
            kind->descriptor.Level == 4U &&
            kind->descriptor.Opcode == 1U &&
            kind->descriptor.Task == 5U &&
            kind->descriptor.Keyword == 0x123456789ABCDEF0ULL,
            "Complete event identity is retained",
            &failures
        );
    }

    event_record.EventHeader.EventDescriptor.Opcode = 2U;
    status = comtrace_event_catalog_add(&catalog, &event_record);

    check_condition(
        status == COMTRACE_EVENT_CATALOG_SUCCESS &&
        comtrace_event_catalog_count(&catalog) == 2U,
        "Different opcode creates a new kind",
        &failures
    );

    event_record.EventHeader.EventDescriptor.Opcode = 1U;
    event_record.EventHeader.EventDescriptor.Version = 3U;
    status = comtrace_event_catalog_add(&catalog, &event_record);

    check_condition(
        status == COMTRACE_EVENT_CATALOG_SUCCESS &&
        comtrace_event_catalog_count(&catalog) == 3U,
        "Different version creates a new kind",
        &failures
    );

    event_record.EventHeader.EventDescriptor.Version = 2U;
    event_record.EventHeader.EventDescriptor.Id = 43U;
    status = comtrace_event_catalog_add(&catalog, &event_record);

    check_condition(
        status == COMTRACE_EVENT_CATALOG_SUCCESS &&
        comtrace_event_catalog_count(&catalog) == 4U,
        "Different event ID creates a new kind",
        &failures
    );

    event_record.EventHeader.EventDescriptor.Id = 42U;
    event_record.EventHeader.ProviderId.Data1 = 0x87654321UL;
    status = comtrace_event_catalog_add(&catalog, &event_record);

    check_condition(
        status == COMTRACE_EVENT_CATALOG_SUCCESS &&
        comtrace_event_catalog_count(&catalog) == 5U,
        "Different provider creates a new kind",
        &failures
    );

    event_record.EventHeader.ProviderId.Data1 = 0x12345678UL;

    event_record.EventHeader.EventDescriptor.Channel = 6U;
    status = comtrace_event_catalog_add(&catalog, &event_record);

    check_condition(
        status == COMTRACE_EVENT_CATALOG_SUCCESS &&
        comtrace_event_catalog_count(&catalog) == 6U,
        "Different channel creates a new kind",
        &failures
    );

    event_record.EventHeader.EventDescriptor.Channel = 3U;
    event_record.EventHeader.EventDescriptor.Level = 7U;
    status = comtrace_event_catalog_add(&catalog, &event_record);

    check_condition(
        status == COMTRACE_EVENT_CATALOG_SUCCESS &&
        comtrace_event_catalog_count(&catalog) == 7U,
        "Different level creates a new kind",
        &failures
    );

    event_record.EventHeader.EventDescriptor.Level = 4U;
    event_record.EventHeader.EventDescriptor.Task = 8U;
    status = comtrace_event_catalog_add(&catalog, &event_record);

    check_condition(
        status == COMTRACE_EVENT_CATALOG_SUCCESS &&
        comtrace_event_catalog_count(&catalog) == 8U,
        "Different task creates a new kind",
        &failures
    );

    event_record.EventHeader.EventDescriptor.Task = 5U;
    event_record.EventHeader.EventDescriptor.Keyword = 0x1111111111111111ULL;
    status = comtrace_event_catalog_add(&catalog, &event_record);

    check_condition(
        status == COMTRACE_EVENT_CATALOG_SUCCESS &&
        comtrace_event_catalog_count(&catalog) == 9U,
        "Different keyword creates a new kind",
        &failures
    );

    comtrace_event_catalog_destroy(&catalog);

    check_condition(
        catalog.items == NULL &&
        catalog.count == 0U &&
        catalog.capacity == 0U,
        "Destroy resets catalog",
        &failures
    );

    return failures;
}

static int test_dynamic_growth(void)
{
    COMTRACE_EVENT_CATALOG catalog;
    EVENT_RECORD event_record;
    COMTRACE_EVENT_CATALOG_STATUS status;
    const COMTRACE_EVENT_KIND *kind;
    ULONG index;
    int failures = 0;

    comtrace_event_catalog_init(&catalog);
    memset(&event_record, 0, sizeof(event_record));

    for (index = 0U; index < 600U; ++index) {
        event_record.EventHeader.ProviderId.Data1 = 1000UL + index;
        event_record.EventHeader.EventDescriptor.Id = (USHORT)index;
        event_record.EventHeader.EventDescriptor.Version = 0U;
        event_record.EventHeader.EventDescriptor.Channel = 0U;
        event_record.EventHeader.EventDescriptor.Level = 0U;
        event_record.EventHeader.EventDescriptor.Opcode = 1U;
        event_record.EventHeader.EventDescriptor.Task = 0U;
        event_record.EventHeader.EventDescriptor.Keyword = 0U;

        status = comtrace_event_catalog_add(&catalog, &event_record);

        if (status != COMTRACE_EVENT_CATALOG_SUCCESS) {
            check_condition(
                0,
                "Catalog accepts more than 512 event kinds",
                &failures
            );
            break;
        }
    }

    check_condition(
        comtrace_event_catalog_count(&catalog) == 600U,
        "Catalog stores 600 distinct event kinds",
        &failures
    );

    kind = comtrace_event_catalog_get(&catalog, 599U);

    check_condition(
        kind != NULL &&
        kind->provider_id.Data1 == 1599UL &&
        kind->descriptor.Id == 599U,
        "Catalog retrieves event kinds after dynamic growth",
        &failures
    );

    check_condition(
        comtrace_event_catalog_get(&catalog, 600U) == NULL,
        "Out-of-range access returns null",
        &failures
    );

    comtrace_event_catalog_destroy(&catalog);
    return failures;
}

static int test_invalid_arguments(void)
{
    COMTRACE_EVENT_CATALOG catalog;
    EVENT_RECORD event_record;
    COMTRACE_EVENT_CATALOG_STATUS status;
    int failures = 0;

    memset(&event_record, 0, sizeof(event_record));
    comtrace_event_catalog_init(&catalog);

    status = comtrace_event_catalog_add(NULL, &event_record);

    check_condition(
        status == COMTRACE_EVENT_CATALOG_INVALID_ARGUMENT,
        "Null catalog is rejected",
        &failures
    );

    status = comtrace_event_catalog_add(&catalog, NULL);

    check_condition(
        status == COMTRACE_EVENT_CATALOG_INVALID_ARGUMENT,
        "Null event is rejected",
        &failures
    );

    check_condition(
        comtrace_event_catalog_count(NULL) == 0U,
        "Null catalog count is zero",
        &failures
    );

    check_condition(
        comtrace_event_catalog_get(NULL, 0U) == NULL,
        "Null catalog lookup returns null",
        &failures
    );

    check_condition(
        comtrace_event_catalog_get(&catalog, 0U) == NULL,
        "Empty catalog lookup returns null",
        &failures
    );

    comtrace_event_catalog_destroy(NULL);
    comtrace_event_catalog_destroy(&catalog);

    return failures;
}

int main(void)
{
    int failures = 0;

    failures += test_catalog_counting();
    failures += test_dynamic_growth();
    failures += test_invalid_arguments();

    if (failures != 0) {
        fprintf(stderr, "%d event catalog test(s) failed\n", failures);
        return 1;
    }

    printf("Event catalog tests: PASS\n");
    return 0;
}