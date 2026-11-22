#ifndef COMTRACE_DDE_TIMELINE_H
#define COMTRACE_DDE_TIMELINE_H

#include <stddef.h>
#include <windows.h>

typedef enum {
    COMTRACE_DDE_TIMELINE_SUCCESS = 0,
    COMTRACE_DDE_TIMELINE_INVALID_ARGUMENT,
    COMTRACE_DDE_TIMELINE_ALLOCATION_FAILED
} COMTRACE_DDE_TIMELINE_STATUS;

typedef enum {
    COMTRACE_DDE_ROLE_CLIENT = 1,
    COMTRACE_DDE_ROLE_SERVER
} COMTRACE_DDE_ROLE;

typedef enum {
    COMTRACE_DDE_TIMELINE_CONVERSATION_OPENED = 1,
    COMTRACE_DDE_TIMELINE_CONVERSATION_CLOSED,
    COMTRACE_DDE_TIMELINE_TRANSACTION_STARTED,
    COMTRACE_DDE_TIMELINE_TRANSACTION_COMPLETED,
    COMTRACE_DDE_TIMELINE_CALLBACK_RECEIVED
} COMTRACE_DDE_TIMELINE_EVENT_TYPE;

typedef struct {
    COMTRACE_DDE_TIMELINE_EVENT_TYPE type;
    DWORD process_id;
    DWORD thread_id;
    ULONGLONG timestamp;
    ULONGLONG conversation_id;
    ULONGLONG conversation_correlation_id;
    ULONGLONG transaction_instance_id;
    ULONGLONG transaction_correlation_id;
    ULONG dde_transaction_id;
    ULONG transaction_type;
    ULONG format;
    BOOL asynchronous;
    COMTRACE_DDE_ROLE role;
    const wchar_t *item;
    ULONG result;
    ULONG data_size;
    BOOL succeeded;
    const wchar_t *service;
    const wchar_t *topic;
} COMTRACE_DDE_TIMELINE_EVENT;

typedef struct {
    DWORD process_id;
    DWORD thread_id;
    ULONGLONG conversation_id;
    ULONGLONG conversation_correlation_id;
    ULONGLONG transaction_instance_id;
    ULONGLONG transaction_correlation_id;
    ULONG dde_transaction_id;
    ULONG transaction_type;
    ULONG format;
    BOOL asynchronous;
    COMTRACE_DDE_ROLE role;
    wchar_t item[256];
    ULONGLONG start_timestamp;
    ULONGLONG completion_timestamp;
    ULONGLONG callback_timestamp;
    ULONGLONG duration_ticks;
    BOOL has_start;
    BOOL has_completion;
    BOOL has_callback;
    BOOL has_duration;
    ULONG result;
    ULONG data_size;
    BOOL succeeded;
    wchar_t service[256];
    wchar_t topic[256];
    ULONGLONG opened_timestamp;
    ULONGLONG closed_timestamp;
    ULONG close_result;
    BOOL has_opened;
    BOOL has_closed;
} COMTRACE_DDE_TIMELINE_RECORD;

typedef struct {
    size_t client_record_index;
    size_t server_record_index;
    ULONGLONG transaction_correlation_id;
} COMTRACE_DDE_EXCHANGE;

typedef struct {
    COMTRACE_DDE_TIMELINE_RECORD *records;
    size_t record_count;
    size_t record_capacity;
    COMTRACE_DDE_EXCHANGE *exchanges;
    size_t exchange_count;
    size_t exchange_capacity;
    ULONGLONG unmatched_completions;
    ULONGLONG missing_transaction_correlation_ids;
    ULONGLONG ambiguous_correlations;
} COMTRACE_DDE_TIMELINE;

void comtrace_dde_timeline_init(COMTRACE_DDE_TIMELINE *timeline);
void comtrace_dde_timeline_destroy(COMTRACE_DDE_TIMELINE *timeline);

COMTRACE_DDE_TIMELINE_STATUS comtrace_dde_timeline_add_event(
    COMTRACE_DDE_TIMELINE *timeline,
    const COMTRACE_DDE_TIMELINE_EVENT *event
);

COMTRACE_DDE_TIMELINE_STATUS comtrace_dde_timeline_correlate(
    COMTRACE_DDE_TIMELINE *timeline
);

size_t comtrace_dde_timeline_count(const COMTRACE_DDE_TIMELINE *timeline);
size_t comtrace_dde_timeline_exchange_count(const COMTRACE_DDE_TIMELINE *timeline);

const COMTRACE_DDE_TIMELINE_RECORD *comtrace_dde_timeline_get(
    const COMTRACE_DDE_TIMELINE *timeline,
    size_t index
);

const COMTRACE_DDE_EXCHANGE *comtrace_dde_timeline_get_exchange(
    const COMTRACE_DDE_TIMELINE *timeline,
    size_t index
);

#endif
