#ifndef COMTRACE_COM_IDENTITY_H
#define COMTRACE_COM_IDENTITY_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <evntcons.h>

typedef enum {
    COMTRACE_IDENTITY_SUCCESS = 0,
    COMTRACE_IDENTITY_INVALID_ARGUMENT,
    COMTRACE_IDENTITY_METADATA_UNAVAILABLE,
    COMTRACE_IDENTITY_PROPERTY_NOT_FOUND,
    COMTRACE_IDENTITY_PROPERTY_UNSUPPORTED,
    COMTRACE_IDENTITY_VALUE_UNAVAILABLE
} COMTRACE_IDENTITY_STATUS;

COMTRACE_IDENTITY_STATUS comtrace_extract_guid_property(
    const EVENT_RECORD *event_record,
    const wchar_t *property_name,
    GUID *value
);

COMTRACE_IDENTITY_STATUS comtrace_extract_uint32_property(
    const EVENT_RECORD *event_record,
    const wchar_t *property_name,
    ULONG *value
);

typedef struct {
    GUID activity_id;
    GUID related_activity_id;
    BOOL has_activity_id;
    BOOL has_related_activity_id;
} COMTRACE_ACTIVITY_IDS;

COMTRACE_IDENTITY_STATUS comtrace_extract_activity_ids(
    const EVENT_RECORD *event_record,
    COMTRACE_ACTIVITY_IDS *activity_ids
);

BOOL comtrace_get_process_image_path(
    DWORD process_id,
    wchar_t *path,
    DWORD path_capacity
);

#endif
