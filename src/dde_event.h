#ifndef COMTRACE_DDE_EVENT_H
#define COMTRACE_DDE_EVENT_H

#include <windows.h>

typedef enum {
    COMTRACE_DDE_EVENT_UNKNOWN = 0,
    COMTRACE_DDE_EVENT_CONVERSATION_OPENED,
    COMTRACE_DDE_EVENT_CONVERSATION_CLOSED,
    COMTRACE_DDE_EVENT_TRANSACTION_STARTED,
    COMTRACE_DDE_EVENT_TRANSACTION_COMPLETED,
    COMTRACE_DDE_EVENT_CALLBACK_RECEIVED
} COMTRACE_DDE_EVENT_TYPE;

BOOL comtrace_is_dde_provider(const GUID *provider_id);
BOOL comtrace_classify_dde_event(
    const GUID *provider_id,
    const wchar_t *event_name,
    COMTRACE_DDE_EVENT_TYPE *event_type
);
const wchar_t *comtrace_dde_event_name(COMTRACE_DDE_EVENT_TYPE event_type);

#endif
