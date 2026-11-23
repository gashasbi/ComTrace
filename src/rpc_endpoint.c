#include "rpc_endpoint.h"

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static void increment_count(ULONGLONG *value)
{
    if (*value != (ULONGLONG)-1) {
        ++(*value);
    }
}

static int ensure_capacity(COMTRACE_RPC_ENDPOINT_CATALOG *catalog)
{
    size_t capacity;
    COMTRACE_RPC_ENDPOINT_RECORD *records;

    if (catalog->record_count < catalog->record_capacity) {
        return 1;
    }

    capacity = catalog->record_capacity == 0U
        ? 16U
        : catalog->record_capacity * 2U;
    if (capacity < catalog->record_capacity ||
        capacity > (size_t)-1 / sizeof(*records)) {
        return 0;
    }

    records = (COMTRACE_RPC_ENDPOINT_RECORD *)realloc(
        catalog->records,
        capacity * sizeof(*records)
    );
    if (records == NULL) {
        return 0;
    }

    catalog->records = records;
    catalog->record_capacity = capacity;
    return 1;
}

static int copy_optional_string(
    wchar_t *destination,
    size_t capacity,
    const wchar_t *source
)
{
    size_t length;

    if (source == NULL || source[0] == L'\0') {
        return 1;
    }

    length = wcslen(source);
    if (length >= capacity) {
        return 0;
    }

    CopyMemory(destination, source, (length + 1U) * sizeof(wchar_t));
    return 1;
}

static COMTRACE_RPC_ENDPOINT_LOCATION get_location(const wchar_t *protocol)
{
    if (protocol == NULL) {
        return COMTRACE_RPC_ENDPOINT_LOCATION_UNKNOWN;
    }
    if (_wcsicmp(protocol, L"ncalrpc") == 0) {
        return COMTRACE_RPC_ENDPOINT_LOCATION_LOCAL;
    }
    if (_wcsicmp(protocol, L"ncacn_ip_tcp") == 0 ||
        _wcsicmp(protocol, L"ncacn_http") == 0) {
        return COMTRACE_RPC_ENDPOINT_LOCATION_NETWORK;
    }
    return COMTRACE_RPC_ENDPOINT_LOCATION_UNKNOWN;
}

static int record_matches(
    const COMTRACE_RPC_ENDPOINT_RECORD *record,
    const COMTRACE_RPC_ENDPOINT_EVENT *event
)
{
    BOOL has_protocol = event->protocol != NULL && event->protocol[0] != L'\0';
    BOOL has_endpoint = event->endpoint != NULL && event->endpoint[0] != L'\0';

    if (record->has_object_uuid != event->has_object_uuid ||
        memcmp(&record->interface_uuid, &event->interface_uuid, sizeof(GUID)) != 0 ||
        (event->has_object_uuid &&
         memcmp(&record->object_uuid, &event->object_uuid, sizeof(GUID)) != 0)) {
        return 0;
    }

    if (_wcsicmp(
            record->protocol,
            has_protocol ? event->protocol : L""
        ) != 0 ||
        _wcsicmp(
            record->endpoint,
            has_endpoint ? event->endpoint : L""
        ) != 0) {
        return 0;
    }

    return 1;
}

static COMTRACE_RPC_ENDPOINT_STATUS append_record(
    COMTRACE_RPC_ENDPOINT_CATALOG *catalog,
    const COMTRACE_RPC_ENDPOINT_EVENT *event,
    COMTRACE_RPC_ENDPOINT_RECORD **record
)
{
    COMTRACE_RPC_ENDPOINT_RECORD *created;

    if (!ensure_capacity(catalog)) {
        return COMTRACE_RPC_ENDPOINT_ALLOCATION_FAILED;
    }

    created = &catalog->records[catalog->record_count];
    ZeroMemory(created, sizeof(*created));
    created->event_process_id = event->event_process_id;
    created->interface_uuid = event->interface_uuid;
    created->has_object_uuid = event->has_object_uuid;
    if (event->has_object_uuid) {
        created->object_uuid = event->object_uuid;
    }
    if (!copy_optional_string(
            created->protocol,
            _countof(created->protocol),
            event->protocol) ||
        !copy_optional_string(
            created->endpoint,
            _countof(created->endpoint),
            event->endpoint)) {
        return COMTRACE_RPC_ENDPOINT_INVALID_ARGUMENT;
    }
    created->location = get_location(created->protocol);
    created->observation_count = 1ULL;
    ++catalog->record_count;
    *record = created;
    return COMTRACE_RPC_ENDPOINT_SUCCESS;
}

void comtrace_rpc_endpoint_catalog_init(COMTRACE_RPC_ENDPOINT_CATALOG *catalog)
{
    if (catalog != NULL) {
        ZeroMemory(catalog, sizeof(*catalog));
    }
}

void comtrace_rpc_endpoint_catalog_destroy(COMTRACE_RPC_ENDPOINT_CATALOG *catalog)
{
    if (catalog == NULL) {
        return;
    }

    free(catalog->records);
    ZeroMemory(catalog, sizeof(*catalog));
}

COMTRACE_RPC_ENDPOINT_STATUS comtrace_rpc_endpoint_catalog_add(
    COMTRACE_RPC_ENDPOINT_CATALOG *catalog,
    const COMTRACE_RPC_ENDPOINT_EVENT *event
)
{
    size_t index;
    COMTRACE_RPC_ENDPOINT_RECORD *record = NULL;
    COMTRACE_RPC_ENDPOINT_STATUS status;

    if (catalog == NULL ||
        event == NULL ||
        (event->phase != COMTRACE_RPC_ENDPOINT_REGISTER &&
         event->phase != COMTRACE_RPC_ENDPOINT_UNREGISTER)) {
        return COMTRACE_RPC_ENDPOINT_INVALID_ARGUMENT;
    }
    {
        GUID empty_guid = {0};
        if (memcmp(&event->interface_uuid, &empty_guid, sizeof(GUID)) == 0) {
            return COMTRACE_RPC_ENDPOINT_INVALID_ARGUMENT;
        }
    }

    if (event->phase == COMTRACE_RPC_ENDPOINT_REGISTER) {
        for (index = catalog->record_count; index > 0U; --index) {
            record = &catalog->records[index - 1U];
            if (record->is_active && record_matches(record, event)) {
                increment_count(&record->observation_count);
                increment_count(&catalog->duplicate_registrations);
                return COMTRACE_RPC_ENDPOINT_SUCCESS;
            }
        }

        status = append_record(catalog, event, &record);
        if (status != COMTRACE_RPC_ENDPOINT_SUCCESS) {
            return status;
        }
        record->has_registration = TRUE;
        record->is_active = TRUE;
        record->register_timestamp = event->timestamp;
        return COMTRACE_RPC_ENDPOINT_SUCCESS;
    }

    for (index = catalog->record_count; index > 0U; --index) {
        record = &catalog->records[index - 1U];
        if (record->is_active &&
            record_matches(record, event) &&
            event->timestamp >= record->register_timestamp) {
            increment_count(&record->observation_count);
            record->has_unregistration = TRUE;
            record->is_active = FALSE;
            record->unregister_timestamp = event->timestamp;
            return COMTRACE_RPC_ENDPOINT_SUCCESS;
        }
    }

    status = append_record(catalog, event, &record);
    if (status != COMTRACE_RPC_ENDPOINT_SUCCESS) {
        return status;
    }
    record->has_unregistration = TRUE;
    record->unregister_timestamp = event->timestamp;
    increment_count(&catalog->unmatched_unregistrations);
    return COMTRACE_RPC_ENDPOINT_SUCCESS;
}

size_t comtrace_rpc_endpoint_catalog_count(
    const COMTRACE_RPC_ENDPOINT_CATALOG *catalog
)
{
    return catalog == NULL ? 0U : catalog->record_count;
}

const COMTRACE_RPC_ENDPOINT_RECORD *comtrace_rpc_endpoint_catalog_get(
    const COMTRACE_RPC_ENDPOINT_CATALOG *catalog,
    size_t index
)
{
    if (catalog == NULL || index >= catalog->record_count) {
        return NULL;
    }

    return &catalog->records[index];
}

COMTRACE_RPC_ENDPOINT_STATUS comtrace_rpc_endpoint_catalog_find(
    const COMTRACE_RPC_ENDPOINT_CATALOG *catalog,
    const GUID *interface_uuid,
    const wchar_t *protocol,
    const wchar_t *endpoint,
    ULONGLONG timestamp,
    size_t *record_index,
    BOOL *found,
    BOOL *ambiguous
)
{
    size_t index;
    size_t matches = 0U;
    size_t match_index = 0U;

    if (record_index != NULL) {
        *record_index = 0U;
    }
    if (found != NULL) {
        *found = FALSE;
    }
    if (ambiguous != NULL) {
        *ambiguous = FALSE;
    }
    if (catalog == NULL || interface_uuid == NULL ||
        protocol == NULL || endpoint == NULL ||
        record_index == NULL || found == NULL || ambiguous == NULL) {
        return COMTRACE_RPC_ENDPOINT_INVALID_ARGUMENT;
    }
    {
        GUID empty_guid = {0};
        if (memcmp(interface_uuid, &empty_guid, sizeof(GUID)) == 0) {
            return COMTRACE_RPC_ENDPOINT_INVALID_ARGUMENT;
        }
    }

    for (index = 0U; index < catalog->record_count; ++index) {
        const COMTRACE_RPC_ENDPOINT_RECORD *record = &catalog->records[index];
        if (!record->has_registration ||
            memcmp(&record->interface_uuid, interface_uuid, sizeof(GUID)) != 0 ||
            _wcsicmp(record->protocol, protocol) != 0 ||
            _wcsicmp(record->endpoint, endpoint) != 0 ||
            timestamp < record->register_timestamp ||
            (record->has_unregistration &&
             timestamp > record->unregister_timestamp)) {
            continue;
        }
        match_index = index;
        ++matches;
    }

    if (matches > 1U) {
        *ambiguous = TRUE;
        return COMTRACE_RPC_ENDPOINT_SUCCESS;
    }
    if (matches == 1U) {
        *record_index = match_index;
        *found = TRUE;
    }
    return COMTRACE_RPC_ENDPOINT_SUCCESS;
}
