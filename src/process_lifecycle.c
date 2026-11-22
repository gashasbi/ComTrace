#include "process_lifecycle.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "com_identity.h"

static const GUID process_provider_id = {
    0x3d6fa8d0,
    0xfe05,
    0x11d0,
    { 0x9d, 0xda, 0x00, 0xc0, 0x4f, 0xd7, 0xba, 0x7c }
};

static COMTRACE_PROCESS_EVENT_TYPE get_event_type(USHORT opcode)
{
    switch (opcode) {
    case 1U:
        return COMTRACE_PROCESS_EVENT_START;
    case 2U:
        return COMTRACE_PROCESS_EVENT_END;
    case 3U:
        return COMTRACE_PROCESS_EVENT_DC_START;
    case 4U:
        return COMTRACE_PROCESS_EVENT_DC_END;
    case 39U:
        return COMTRACE_PROCESS_EVENT_DEFUNCT;
    default:
        return COMTRACE_PROCESS_EVENT_UNKNOWN;
    }
}

BOOL comtrace_process_lifecycle_is_event(
    const EVENT_RECORD *event_record
)
{
    if (event_record == NULL ||
        memcmp(
            &event_record->EventHeader.ProviderId,
            &process_provider_id,
            sizeof(process_provider_id)
        ) != 0) {
        return FALSE;
    }

    return get_event_type(
        event_record->EventHeader.EventDescriptor.Opcode
    ) != COMTRACE_PROCESS_EVENT_UNKNOWN;
}

COMTRACE_PROCESS_LIFECYCLE_STATUS comtrace_process_lifecycle_extract(
    const EVENT_RECORD *event_record,
    COMTRACE_PROCESS_EVENT *process_event
)
{
    COMTRACE_PROCESS_EVENT result;
    COMTRACE_IDENTITY_STATUS identity_status;

    if (event_record == NULL || process_event == NULL) {
        return COMTRACE_PROCESS_LIFECYCLE_INVALID_ARGUMENT;
    }

    result.type = get_event_type(
        event_record->EventHeader.EventDescriptor.Opcode
    );

    if (!comtrace_process_lifecycle_is_event(event_record) ||
        result.type == COMTRACE_PROCESS_EVENT_UNKNOWN) {
        return COMTRACE_PROCESS_LIFECYCLE_INVALID_ARGUMENT;
    }

    ZeroMemory(&result, sizeof(result));
    result.type = get_event_type(
        event_record->EventHeader.EventDescriptor.Opcode
    );
    result.timestamp = event_record->EventHeader.TimeStamp.QuadPart;

    identity_status = comtrace_extract_uint32_property(
        event_record,
        L"ProcessId",
        &result.process_id
    );

    if (identity_status != COMTRACE_IDENTITY_SUCCESS ||
        result.process_id == 0U) {
        return COMTRACE_PROCESS_LIFECYCLE_METADATA_UNAVAILABLE;
    }

    identity_status = comtrace_extract_uint32_property(
        event_record,
        L"ParentId",
        &result.parent_process_id
    );

    if (identity_status == COMTRACE_IDENTITY_SUCCESS &&
        result.parent_process_id != 0U) {
        result.has_parent_process_id = TRUE;
    }

    CopyMemory(process_event, &result, sizeof(result));
    return COMTRACE_PROCESS_LIFECYCLE_SUCCESS;
}

void comtrace_process_lifecycle_catalog_init(
    COMTRACE_PROCESS_LIFECYCLE_CATALOG *catalog
)
{
    if (catalog != NULL) {
        ZeroMemory(catalog, sizeof(*catalog));
    }
}

void comtrace_process_lifecycle_catalog_destroy(
    COMTRACE_PROCESS_LIFECYCLE_CATALOG *catalog
)
{
    if (catalog == NULL) {
        return;
    }

    free(catalog->items);
    ZeroMemory(catalog, sizeof(*catalog));
}

static COMTRACE_PROCESS_LIFECYCLE_STATUS ensure_capacity(
    COMTRACE_PROCESS_LIFECYCLE_CATALOG *catalog
)
{
    size_t capacity;
    COMTRACE_PROCESS_LIFETIME *items;

    if (catalog->count < catalog->capacity) {
        return COMTRACE_PROCESS_LIFECYCLE_SUCCESS;
    }

    capacity = catalog->capacity == 0U
        ? 16U
        : catalog->capacity * 2U;

    if (capacity < catalog->capacity ||
        capacity > (size_t)-1 / sizeof(*items)) {
        return COMTRACE_PROCESS_LIFECYCLE_OVERFLOW;
    }

    items = (COMTRACE_PROCESS_LIFETIME *)realloc(
        catalog->items,
        capacity * sizeof(*items)
    );

    if (items == NULL) {
        return COMTRACE_PROCESS_LIFECYCLE_ALLOCATION_FAILED;
    }

    catalog->items = items;
    catalog->capacity = capacity;
    return COMTRACE_PROCESS_LIFECYCLE_SUCCESS;
}

static ULONGLONG next_generation(
    const COMTRACE_PROCESS_LIFECYCLE_CATALOG *catalog,
    DWORD process_id
)
{
    ULONGLONG generation = 0ULL;
    size_t index;

    for (index = 0U; index < catalog->count; ++index) {
        const COMTRACE_PROCESS_LIFETIME *lifetime =
            &catalog->items[index];

        if (lifetime->process_id == process_id &&
            lifetime->generation > generation) {
            generation = lifetime->generation;
        }
    }

    return generation == ULLONG_MAX ? 0ULL : generation + 1ULL;
}

static COMTRACE_PROCESS_LIFECYCLE_STATUS append_lifetime(
    COMTRACE_PROCESS_LIFECYCLE_CATALOG *catalog,
    DWORD process_id,
    COMTRACE_PROCESS_LIFETIME **lifetime
)
{
    COMTRACE_PROCESS_LIFECYCLE_STATUS status;
    ULONGLONG generation;

    generation = next_generation(catalog, process_id);
    if (generation == 0ULL) {
        return COMTRACE_PROCESS_LIFECYCLE_OVERFLOW;
    }

    status = ensure_capacity(catalog);
    if (status != COMTRACE_PROCESS_LIFECYCLE_SUCCESS) {
        return status;
    }

    *lifetime = &catalog->items[catalog->count];
    ZeroMemory(*lifetime, sizeof(**lifetime));
    (*lifetime)->process_id = process_id;
    (*lifetime)->generation = generation;
    ++catalog->count;
    return COMTRACE_PROCESS_LIFECYCLE_SUCCESS;
}

static COMTRACE_PROCESS_LIFETIME *find_open_lifetime(
    COMTRACE_PROCESS_LIFECYCLE_CATALOG *catalog,
    DWORD process_id,
    LONGLONG timestamp
)
{
    COMTRACE_PROCESS_LIFETIME *match = NULL;
    size_t index;

    for (index = 0U; index < catalog->count; ++index) {
        COMTRACE_PROCESS_LIFETIME *current = &catalog->items[index];

        if (current->process_id != process_id ||
            current->has_end_timestamp ||
            !current->has_first_seen_timestamp ||
            current->first_seen_timestamp > timestamp) {
            continue;
        }

        if (match == NULL ||
            current->generation > match->generation) {
            match = current;
        }
    }

    return match;
}

static int has_duplicate_start(
    const COMTRACE_PROCESS_LIFECYCLE_CATALOG *catalog,
    const COMTRACE_PROCESS_EVENT *process_event,
    BOOL rundown
)
{
    size_t index;

    for (index = 0U; index < catalog->count; ++index) {
        const COMTRACE_PROCESS_LIFETIME *current =
            &catalog->items[index];

        if (current->process_id == process_event->process_id &&
            current->has_first_seen_timestamp &&
            current->first_seen_timestamp == process_event->timestamp &&
            current->started_during_trace != rundown) {
            return 1;
        }
    }

    return 0;
}

static COMTRACE_PROCESS_LIFECYCLE_STATUS count_event(
    ULONGLONG *count
)
{
    if (*count == ULLONG_MAX) {
        return COMTRACE_PROCESS_LIFECYCLE_OVERFLOW;
    }

    ++(*count);
    return COMTRACE_PROCESS_LIFECYCLE_SUCCESS;
}

COMTRACE_PROCESS_LIFECYCLE_STATUS comtrace_process_lifecycle_catalog_add(
    COMTRACE_PROCESS_LIFECYCLE_CATALOG *catalog,
    const COMTRACE_PROCESS_EVENT *process_event
)
{
    COMTRACE_PROCESS_LIFETIME *lifetime;
    COMTRACE_PROCESS_LIFECYCLE_STATUS status;
    BOOL rundown;

    if (catalog == NULL ||
        process_event == NULL ||
        process_event->process_id == 0U) {
        return COMTRACE_PROCESS_LIFECYCLE_INVALID_ARGUMENT;
    }

    switch (process_event->type) {
    case COMTRACE_PROCESS_EVENT_START:
    case COMTRACE_PROCESS_EVENT_DC_START:
        rundown = process_event->type == COMTRACE_PROCESS_EVENT_DC_START;
        status = count_event(
            rundown ? &catalog->dc_start_events : &catalog->start_events
        );
        if (status != COMTRACE_PROCESS_LIFECYCLE_SUCCESS) {
            return status;
        }

        if (has_duplicate_start(catalog, process_event, rundown)) {
            return COMTRACE_PROCESS_LIFECYCLE_SUCCESS;
        }

        status = append_lifetime(
            catalog,
            process_event->process_id,
            &lifetime
        );
        if (status != COMTRACE_PROCESS_LIFECYCLE_SUCCESS) {
            return status;
        }

        lifetime->first_seen_timestamp = process_event->timestamp;
        lifetime->has_first_seen_timestamp = TRUE;
        lifetime->started_during_trace = !rundown;
        if (process_event->has_parent_process_id) {
            lifetime->parent_process_id =
                process_event->parent_process_id;
            lifetime->has_parent_process_id = TRUE;
        }
        return COMTRACE_PROCESS_LIFECYCLE_SUCCESS;

    case COMTRACE_PROCESS_EVENT_END:
        status = count_event(&catalog->end_events);
        if (status != COMTRACE_PROCESS_LIFECYCLE_SUCCESS) {
            return status;
        }

        for (size_t index = 0U; index < catalog->count; ++index) {
            lifetime = &catalog->items[index];
            if (lifetime->process_id == process_event->process_id &&
                lifetime->has_end_timestamp &&
                lifetime->end_timestamp == process_event->timestamp) {
                return COMTRACE_PROCESS_LIFECYCLE_SUCCESS;
            }
        }

        lifetime = find_open_lifetime(
            catalog,
            process_event->process_id,
            process_event->timestamp
        );

        if (lifetime == NULL) {
            status = append_lifetime(
                catalog,
                process_event->process_id,
                &lifetime
            );
            if (status != COMTRACE_PROCESS_LIFECYCLE_SUCCESS) {
                return status;
            }
        }

        lifetime->end_timestamp = process_event->timestamp;
        lifetime->has_end_timestamp = TRUE;
        return COMTRACE_PROCESS_LIFECYCLE_SUCCESS;

    case COMTRACE_PROCESS_EVENT_DC_END:
        status = count_event(&catalog->dc_end_events);
        if (status != COMTRACE_PROCESS_LIFECYCLE_SUCCESS) {
            return status;
        }

        lifetime = find_open_lifetime(
            catalog,
            process_event->process_id,
            process_event->timestamp
        );

        if (lifetime == NULL) {
            status = append_lifetime(
                catalog,
                process_event->process_id,
                &lifetime
            );
            if (status != COMTRACE_PROCESS_LIFECYCLE_SUCCESS) {
                return status;
            }
        }

        lifetime->trace_end_timestamp = process_event->timestamp;
        lifetime->has_trace_end_timestamp = TRUE;
        return COMTRACE_PROCESS_LIFECYCLE_SUCCESS;

    case COMTRACE_PROCESS_EVENT_DEFUNCT:
        return count_event(&catalog->defunct_events);

    default:
        return COMTRACE_PROCESS_LIFECYCLE_INVALID_ARGUMENT;
    }
}
