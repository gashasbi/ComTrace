#ifndef COMTRACE_EVENT_CATALOG_H
#define COMTRACE_EVENT_CATALOG_H

#include <stddef.h>
#include <windows.h>
#include <evntcons.h>

typedef struct {
    GUID provider_id;
    EVENT_DESCRIPTOR descriptor;
    ULONGLONG count;
} COMTRACE_EVENT_KIND;

typedef struct {
    COMTRACE_EVENT_KIND *items;
    size_t count;
    size_t capacity;
} COMTRACE_EVENT_CATALOG;

typedef enum {
    COMTRACE_EVENT_CATALOG_SUCCESS = 0,
    COMTRACE_EVENT_CATALOG_INVALID_ARGUMENT,
    COMTRACE_EVENT_CATALOG_ALLOCATION_FAILED,
    COMTRACE_EVENT_CATALOG_CAPACITY_OVERFLOW,
    COMTRACE_EVENT_CATALOG_COUNT_OVERFLOW
} COMTRACE_EVENT_CATALOG_STATUS;

void comtrace_event_catalog_init(COMTRACE_EVENT_CATALOG *catalog);

COMTRACE_EVENT_CATALOG_STATUS comtrace_event_catalog_add(
    COMTRACE_EVENT_CATALOG *catalog,
    const EVENT_RECORD *event_record
);

size_t comtrace_event_catalog_count(
    const COMTRACE_EVENT_CATALOG *catalog
);

const COMTRACE_EVENT_KIND *comtrace_event_catalog_get(
    const COMTRACE_EVENT_CATALOG *catalog,
    size_t index
);

void comtrace_event_catalog_destroy(COMTRACE_EVENT_CATALOG *catalog);

#endif