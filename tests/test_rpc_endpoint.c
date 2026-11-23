#include "rpc_endpoint.h"

#include <stdio.h>
#include <string.h>
#include <wchar.h>

static const GUID interface_id = {
    0x12345678, 0x1234, 0x5678, {1, 2, 3, 4, 5, 6, 7, 8}
};

static const GUID object_id = {
    0x87654321, 0x4321, 0x8765, {8, 7, 6, 5, 4, 3, 2, 1}
};

static int test_registration_lifecycle_and_protocol_scope(void)
{
    COMTRACE_RPC_ENDPOINT_CATALOG catalog;
    COMTRACE_RPC_ENDPOINT_EVENT event;
    const COMTRACE_RPC_ENDPOINT_RECORD *local;
    const COMTRACE_RPC_ENDPOINT_RECORD *network;

    comtrace_rpc_endpoint_catalog_init(&catalog);
    ZeroMemory(&event, sizeof(event));
    event.phase = COMTRACE_RPC_ENDPOINT_REGISTER;
    event.event_process_id = 4U;
    event.timestamp = 100ULL;
    event.interface_uuid = interface_id;
    event.has_object_uuid = TRUE;
    event.object_uuid = object_id;
    event.protocol = L"ncalrpc";
    event.endpoint = L"LRPC-123";
    if (comtrace_rpc_endpoint_catalog_add(&catalog, &event) !=
        COMTRACE_RPC_ENDPOINT_SUCCESS) {
        comtrace_rpc_endpoint_catalog_destroy(&catalog);
        return 0;
    }

    event.protocol = L"ncacn_ip_tcp";
    event.endpoint = L"49664";
    event.timestamp = 110ULL;
    if (comtrace_rpc_endpoint_catalog_add(&catalog, &event) !=
        COMTRACE_RPC_ENDPOINT_SUCCESS) {
        comtrace_rpc_endpoint_catalog_destroy(&catalog);
        return 0;
    }

    event.phase = COMTRACE_RPC_ENDPOINT_UNREGISTER;
    event.timestamp = 120ULL;
    if (comtrace_rpc_endpoint_catalog_add(&catalog, &event) !=
        COMTRACE_RPC_ENDPOINT_SUCCESS) {
        comtrace_rpc_endpoint_catalog_destroy(&catalog);
        return 0;
    }

    local = comtrace_rpc_endpoint_catalog_get(&catalog, 0U);
    network = comtrace_rpc_endpoint_catalog_get(&catalog, 1U);
    if (local == NULL || network == NULL ||
        !local->is_active || local->has_unregistration ||
        !network->has_registration || !network->has_unregistration ||
        network->is_active ||
        local->location != COMTRACE_RPC_ENDPOINT_LOCATION_LOCAL ||
        network->location != COMTRACE_RPC_ENDPOINT_LOCATION_NETWORK ||
        wcscmp(local->endpoint, L"LRPC-123") != 0 ||
        wcscmp(network->endpoint, L"49664") != 0 ||
        catalog.unmatched_unregistrations != 0ULL) {
        comtrace_rpc_endpoint_catalog_destroy(&catalog);
        return 0;
    }

    comtrace_rpc_endpoint_catalog_destroy(&catalog);
    return 1;
}

static int test_unmatched_unregistration_does_not_close_other_endpoint(void)
{
    COMTRACE_RPC_ENDPOINT_CATALOG catalog;
    COMTRACE_RPC_ENDPOINT_EVENT event;
    const COMTRACE_RPC_ENDPOINT_RECORD *record;

    comtrace_rpc_endpoint_catalog_init(&catalog);
    ZeroMemory(&event, sizeof(event));
    event.phase = COMTRACE_RPC_ENDPOINT_REGISTER;
    event.interface_uuid = interface_id;
    event.protocol = L"ncacn_ip_tcp";
    event.endpoint = L"50000";
    if (comtrace_rpc_endpoint_catalog_add(&catalog, &event) !=
        COMTRACE_RPC_ENDPOINT_SUCCESS) {
        comtrace_rpc_endpoint_catalog_destroy(&catalog);
        return 0;
    }

    event.phase = COMTRACE_RPC_ENDPOINT_UNREGISTER;
    event.endpoint = L"50001";
    if (comtrace_rpc_endpoint_catalog_add(&catalog, &event) !=
        COMTRACE_RPC_ENDPOINT_SUCCESS) {
        comtrace_rpc_endpoint_catalog_destroy(&catalog);
        return 0;
    }

    record = comtrace_rpc_endpoint_catalog_get(&catalog, 0U);
    if (record == NULL || !record->is_active ||
        catalog.unmatched_unregistrations != 1ULL ||
        comtrace_rpc_endpoint_catalog_count(&catalog) != 2U) {
        comtrace_rpc_endpoint_catalog_destroy(&catalog);
        return 0;
    }

    comtrace_rpc_endpoint_catalog_destroy(&catalog);
    return 1;
}

static int test_time_aware_endpoint_lookup(void)
{
    COMTRACE_RPC_ENDPOINT_CATALOG catalog;
    COMTRACE_RPC_ENDPOINT_EVENT event;
    size_t record_index = 0U;
    BOOL found = FALSE;
    BOOL ambiguous = FALSE;

    comtrace_rpc_endpoint_catalog_init(&catalog);
    ZeroMemory(&event, sizeof(event));
    event.phase = COMTRACE_RPC_ENDPOINT_REGISTER;
    event.interface_uuid = interface_id;
    event.protocol = L"ncacn_ip_tcp";
    event.endpoint = L"50123";
    event.timestamp = 100ULL;
    if (comtrace_rpc_endpoint_catalog_add(&catalog, &event) !=
        COMTRACE_RPC_ENDPOINT_SUCCESS) {
        comtrace_rpc_endpoint_catalog_destroy(&catalog);
        return 0;
    }

    event.phase = COMTRACE_RPC_ENDPOINT_UNREGISTER;
    event.timestamp = 200ULL;
    if (comtrace_rpc_endpoint_catalog_add(&catalog, &event) !=
        COMTRACE_RPC_ENDPOINT_SUCCESS ||
        comtrace_rpc_endpoint_catalog_find(
            &catalog,
            &interface_id,
            L"ncacn_ip_tcp",
            L"50123",
            150ULL,
            &record_index,
            &found,
            &ambiguous
        ) != COMTRACE_RPC_ENDPOINT_SUCCESS ||
        !found || ambiguous || record_index != 0U) {
        comtrace_rpc_endpoint_catalog_destroy(&catalog);
        return 0;
    }

    found = TRUE;
    if (comtrace_rpc_endpoint_catalog_find(
            &catalog,
            &interface_id,
            L"ncacn_ip_tcp",
            L"50123",
            250ULL,
            &record_index,
            &found,
            &ambiguous
        ) != COMTRACE_RPC_ENDPOINT_SUCCESS ||
        found || ambiguous) {
        comtrace_rpc_endpoint_catalog_destroy(&catalog);
        return 0;
    }

    comtrace_rpc_endpoint_catalog_destroy(&catalog);
    return 1;
}

int main(void)
{
    if (!test_registration_lifecycle_and_protocol_scope() ||
        !test_unmatched_unregistration_does_not_close_other_endpoint() ||
        !test_time_aware_endpoint_lookup()) {
        fputs("RPC endpoint tests failed.\n", stderr);
        return 1;
    }

    puts("RPC endpoint tests passed.");
    return 0;
}
