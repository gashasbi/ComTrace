#ifndef COMTRACE_EVENT_RECORD_H
#define COMTRACE_EVENT_RECORD_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <evntcons.h>

typedef enum {
    COMTRACE_EVENT_RECORD_SUCCESS = 0,
    COMTRACE_EVENT_RECORD_INVALID_ARGUMENT,
    COMTRACE_EVENT_RECORD_INVALID_SOURCE,
    COMTRACE_EVENT_RECORD_ALLOCATION_FAILED
} COMTRACE_EVENT_RECORD_STATUS;

typedef struct {
    EVENT_RECORD record;
    EVENT_HEADER_EXTENDED_DATA_ITEM *extended_data;
    BYTE *user_data;
    USHORT extended_data_count;
} COMTRACE_OWNED_EVENT_RECORD;

COMTRACE_EVENT_RECORD_STATUS comtrace_event_record_copy(
    const EVENT_RECORD *source,
    COMTRACE_OWNED_EVENT_RECORD **destination
);

void comtrace_event_record_destroy(
    COMTRACE_OWNED_EVENT_RECORD *record
);

#endif