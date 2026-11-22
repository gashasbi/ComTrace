#ifndef COMTRACE_COM_LIFETIME_ANALYSIS_H
#define COMTRACE_COM_LIFETIME_ANALYSIS_H

#include <stddef.h>
#include <windows.h>

typedef enum {
    COMTRACE_LIFETIME_EVENT_CREATED = 1,
    COMTRACE_LIFETIME_EVENT_ADD_REF,
    COMTRACE_LIFETIME_EVENT_RELEASE,
    COMTRACE_LIFETIME_EVENT_DESTROYED,
    COMTRACE_LIFETIME_EVENT_NATIVE_IDENTITY_BOUND
} COMTRACE_LIFETIME_EVENT_TYPE;

typedef struct {
    GUID interface_id;
} COMTRACE_LIFETIME_INTERFACE_ID;

typedef struct {
    ULONGLONG native_object_id;
    BOOL has_native_ipid;
    GUID native_ipid;
    BOOL has_interface_id;
    GUID interface_id;
} COMTRACE_LIFETIME_NATIVE_IDENTITY;

typedef struct {
    COMTRACE_LIFETIME_EVENT_TYPE type;
    DWORD process_id;
    DWORD thread_id;
    ULONGLONG object_instance_id;
    ULONGLONG process_generation;
    BOOL has_process_generation;
    ULONGLONG timestamp;
    BOOL has_class_id;
    GUID class_id;
    BOOL has_interface_id;
    GUID interface_id;
    BOOL has_reference_count_result;
    ULONG reference_count_result;
    BOOL has_initial_reference_count;
    ULONG initial_reference_count;
    BOOL has_native_object_id;
    ULONGLONG native_object_id;
    BOOL has_native_ipid;
    GUID native_ipid;
} COMTRACE_LIFETIME_EVENT;

typedef struct {
    DWORD process_id;
    ULONGLONG object_instance_id;
    ULONGLONG process_generation;
    BOOL has_process_generation;
    BOOL has_created;
    ULONGLONG created_timestamp;
    BOOL has_destroyed;
    ULONGLONG destroyed_timestamp;
    BOOL has_class_id;
    GUID class_id;
    BOOL has_interface_id;
    GUID interface_id;
    COMTRACE_LIFETIME_INTERFACE_ID *interface_ids;
    size_t interface_id_count;
    size_t interface_id_capacity;
    COMTRACE_LIFETIME_NATIVE_IDENTITY *native_identities;
    size_t native_identity_count;
    size_t native_identity_capacity;
    BOOL has_native_object_id;
    ULONGLONG native_object_id;
    BOOL has_native_ipid;
    GUID native_ipid;
    BOOL has_initial_reference_count;
    ULONG initial_reference_count;
    BOOL has_last_reference_count_result;
    ULONG last_reference_count_result;
    ULONGLONG create_events;
    ULONGLONG add_ref_events;
    ULONGLONG release_events;
    ULONGLONG destroy_events;
    ULONGLONG release_zero_results;
    ULONGLONG observation_count;
    ULONGLONG first_seen_timestamp;
    ULONGLONG last_seen_timestamp;
} COMTRACE_LIFETIME_RECORD;

typedef enum {
    COMTRACE_LIFETIME_SUCCESS = 0,
    COMTRACE_LIFETIME_INVALID_ARGUMENT,
    COMTRACE_LIFETIME_ALLOCATION_FAILED
} COMTRACE_LIFETIME_STATUS;

typedef struct {
    COMTRACE_LIFETIME_RECORD *records;
    size_t record_count;
    size_t record_capacity;
    ULONGLONG events_seen;
    ULONGLONG create_events;
    ULONGLONG add_ref_events;
    ULONGLONG release_events;
    ULONGLONG destroy_events;
    ULONGLONG native_identity_bind_events;
    ULONGLONG events_without_creation;
    ULONGLONG events_after_destruction;
    ULONGLONG duplicate_creations;
    ULONGLONG destructions_without_creation;
    ULONGLONG duplicate_destructions;
} COMTRACE_COM_LIFETIME_ANALYSIS;

void comtrace_lifetime_analysis_init(
    COMTRACE_COM_LIFETIME_ANALYSIS *analysis
);

void comtrace_lifetime_analysis_destroy(
    COMTRACE_COM_LIFETIME_ANALYSIS *analysis
);

COMTRACE_LIFETIME_STATUS comtrace_lifetime_analysis_add_event(
    COMTRACE_COM_LIFETIME_ANALYSIS *analysis,
    const COMTRACE_LIFETIME_EVENT *event,
    const COMTRACE_LIFETIME_RECORD **record
);

size_t comtrace_lifetime_analysis_count(
    const COMTRACE_COM_LIFETIME_ANALYSIS *analysis
);

const COMTRACE_LIFETIME_RECORD *comtrace_lifetime_analysis_get(
    const COMTRACE_COM_LIFETIME_ANALYSIS *analysis,
    size_t index
);

#endif
