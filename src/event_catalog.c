#include "event_catalog.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static int event_descriptor_matches(
    const EVENT_DESCRIPTOR *first,
    const EVENT_DESCRIPTOR *second
)
{
    return first->Id == second->Id &&
        first->Version == second->Version &&
        first->Channel == second->Channel &&
        first->Level == second->Level &&
        first->Opcode == second->Opcode &&
        first->Task == second->Task &&
        first->Keyword == second->Keyword;
}

static int event_kind_matches(
    const COMTRACE_EVENT_KIND *kind,
    const EVENT_RECORD *event_record
)
{
    return memcmp(
        &kind->provider_id,
        &event_record->EventHeader.ProviderId,
        sizeof(GUID)
    ) == 0 &&
    event_descriptor_matches(
        &kind->descriptor,
        &event_record->EventHeader.EventDescriptor
    );
}

void comtrace_event_catalog_init(COMTRACE_EVENT_CATALOG *catalog)
{
    if (catalog == NULL) {
        return;
    }

    memset(catalog, 0, sizeof(*catalog));
}

COMTRACE_EVENT_CATALOG_STATUS comtrace_event_catalog_add(
    COMTRACE_EVENT_CATALOG *catalog,
    const EVENT_RECORD *event_record
)
{
    size_t index;

    if (catalog == NULL || event_record == NULL) {
        return COMTRACE_EVENT_CATALOG_INVALID_ARGUMENT;
    }

    for (index = 0U; index < catalog->count; ++index) {
        COMTRACE_EVENT_KIND *kind = &catalog->items[index];

        if (event_kind_matches(kind, event_record)) {
            if (kind->count == ULLONG_MAX) {
                return COMTRACE_EVENT_CATALOG_COUNT_OVERFLOW;
            }

            ++kind->count;
            return COMTRACE_EVENT_CATALOG_SUCCESS;
        }
    }

    if (catalog->count == catalog->capacity) {
        size_t new_capacity;
        COMTRACE_EVENT_KIND *new_items;

        if (catalog->capacity == 0U) {
            new_capacity = 16U;
        } else {
            if (catalog->capacity > SIZE_MAX / 2U) {
                return COMTRACE_EVENT_CATALOG_CAPACITY_OVERFLOW;
            }

            new_capacity = catalog->capacity * 2U;
        }

        if (new_capacity > SIZE_MAX / sizeof(*catalog->items)) {
            return COMTRACE_EVENT_CATALOG_CAPACITY_OVERFLOW;
        }

        new_items = (COMTRACE_EVENT_KIND *)realloc(
            catalog->items,
            new_capacity * sizeof(*catalog->items)
        );

        if (new_items == NULL) {
            return COMTRACE_EVENT_CATALOG_ALLOCATION_FAILED;
        }

        catalog->items = new_items;
        catalog->capacity = new_capacity;
    }

    {
        COMTRACE_EVENT_KIND *kind = &catalog->items[catalog->count];

        kind->provider_id = event_record->EventHeader.ProviderId;
        kind->descriptor = event_record->EventHeader.EventDescriptor;
        kind->count = 1U;

        ++catalog->count;
    }

    return COMTRACE_EVENT_CATALOG_SUCCESS;
}

size_t comtrace_event_catalog_count(
    const COMTRACE_EVENT_CATALOG *catalog
)
{
    if (catalog == NULL) {
        return 0U;
    }

    return catalog->count;
}

const COMTRACE_EVENT_KIND *comtrace_event_catalog_get(
    const COMTRACE_EVENT_CATALOG *catalog,
    size_t index
)
{
    if (catalog == NULL || index >= catalog->count) {
        return NULL;
    }

    return &catalog->items[index];
}

void comtrace_event_catalog_destroy(COMTRACE_EVENT_CATALOG *catalog)
{
    if (catalog == NULL) {
        return;
    }

    free(catalog->items);
    memset(catalog, 0, sizeof(*catalog));
}