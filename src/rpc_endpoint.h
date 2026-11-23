#ifndef COMTRACE_RPC_ENDPOINT_H
#define COMTRACE_RPC_ENDPOINT_H

#include <stddef.h>
#include "com_identity.h"

typedef enum {
    COMTRACE_RPC_ENDPOINT_SUCCESS = 0,
    COMTRACE_RPC_ENDPOINT_INVALID_ARGUMENT,
    COMTRACE_RPC_ENDPOINT_ALLOCATION_FAILED
} COMTRACE_RPC_ENDPOINT_STATUS;

typedef enum {
    COMTRACE_RPC_ENDPOINT_REGISTER = 1,
    COMTRACE_RPC_ENDPOINT_UNREGISTER
} COMTRACE_RPC_ENDPOINT_PHASE;

typedef enum {
    COMTRACE_RPC_ENDPOINT_LOCATION_UNKNOWN = 0,
    COMTRACE_RPC_ENDPOINT_LOCATION_LOCAL,
    COMTRACE_RPC_ENDPOINT_LOCATION_NETWORK
} COMTRACE_RPC_ENDPOINT_LOCATION;

typedef struct {
    COMTRACE_RPC_ENDPOINT_PHASE phase;
    DWORD event_process_id;
    ULONGLONG timestamp;
    GUID interface_uuid;
    BOOL has_object_uuid;
    GUID object_uuid;
    const wchar_t *protocol;
    const wchar_t *endpoint;
} COMTRACE_RPC_ENDPOINT_EVENT;

typedef struct {
    DWORD event_process_id;
    GUID interface_uuid;
    BOOL has_object_uuid;
    GUID object_uuid;
    wchar_t protocol[64];
    wchar_t endpoint[128];
    COMTRACE_RPC_ENDPOINT_LOCATION location;
    ULONGLONG register_timestamp;
    ULONGLONG unregister_timestamp;
    ULONGLONG observation_count;
    BOOL has_registration;
    BOOL has_unregistration;
    BOOL is_active;
} COMTRACE_RPC_ENDPOINT_RECORD;

typedef struct {
    COMTRACE_RPC_ENDPOINT_RECORD *records;
    size_t record_count;
    size_t record_capacity;
    ULONGLONG unmatched_unregistrations;
    ULONGLONG duplicate_registrations;
} COMTRACE_RPC_ENDPOINT_CATALOG;

void comtrace_rpc_endpoint_catalog_init(COMTRACE_RPC_ENDPOINT_CATALOG *catalog);
void comtrace_rpc_endpoint_catalog_destroy(COMTRACE_RPC_ENDPOINT_CATALOG *catalog);

COMTRACE_RPC_ENDPOINT_STATUS comtrace_rpc_endpoint_catalog_add(
    COMTRACE_RPC_ENDPOINT_CATALOG *catalog,
    const COMTRACE_RPC_ENDPOINT_EVENT *event
);

size_t comtrace_rpc_endpoint_catalog_count(
    const COMTRACE_RPC_ENDPOINT_CATALOG *catalog
);

const COMTRACE_RPC_ENDPOINT_RECORD *comtrace_rpc_endpoint_catalog_get(
    const COMTRACE_RPC_ENDPOINT_CATALOG *catalog,
    size_t index
);

COMTRACE_RPC_ENDPOINT_STATUS comtrace_rpc_endpoint_catalog_find(
    const COMTRACE_RPC_ENDPOINT_CATALOG *catalog,
    const GUID *interface_uuid,
    const wchar_t *protocol,
    const wchar_t *endpoint,
    ULONGLONG timestamp,
    size_t *record_index,
    BOOL *found,
    BOOL *ambiguous
);

#endif
