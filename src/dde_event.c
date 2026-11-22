#include "dde_event.h"

#include <string.h>
#include <wchar.h>

static const GUID dde_provider_id = {
    0x7b6d4a21,
    0x9e35,
    0x4c80,
    {0xa1, 0x72, 0x53, 0x94, 0xb6, 0xd8, 0x0f, 0x21}
};

BOOL comtrace_is_dde_provider(const GUID *provider_id)
{
    return provider_id != NULL &&
        memcmp(provider_id, &dde_provider_id, sizeof(GUID)) == 0;
}

BOOL comtrace_classify_dde_event(
    const GUID *provider_id,
    const wchar_t *event_name,
    COMTRACE_DDE_EVENT_TYPE *event_type
)
{
    COMTRACE_DDE_EVENT_TYPE type = COMTRACE_DDE_EVENT_UNKNOWN;

    if (!comtrace_is_dde_provider(provider_id) ||
        event_name == NULL ||
        event_type == NULL) {
        return FALSE;
    }

    if (wcscmp(event_name, L"DdeConversationOpened") == 0) {
        type = COMTRACE_DDE_EVENT_CONVERSATION_OPENED;
    } else if (wcscmp(event_name, L"DdeConversationClosed") == 0) {
        type = COMTRACE_DDE_EVENT_CONVERSATION_CLOSED;
    } else if (wcscmp(event_name, L"DdeTransactionStarted") == 0) {
        type = COMTRACE_DDE_EVENT_TRANSACTION_STARTED;
    } else if (wcscmp(event_name, L"DdeTransactionCompleted") == 0) {
        type = COMTRACE_DDE_EVENT_TRANSACTION_COMPLETED;
    } else if (wcscmp(event_name, L"DdeCallbackReceived") == 0) {
        type = COMTRACE_DDE_EVENT_CALLBACK_RECEIVED;
    }

    if (type == COMTRACE_DDE_EVENT_UNKNOWN) {
        return FALSE;
    }

    *event_type = type;
    return TRUE;
}

const wchar_t *comtrace_dde_event_name(COMTRACE_DDE_EVENT_TYPE event_type)
{
    switch (event_type) {
    case COMTRACE_DDE_EVENT_CONVERSATION_OPENED:
        return L"DdeConversationOpened";
    case COMTRACE_DDE_EVENT_CONVERSATION_CLOSED:
        return L"DdeConversationClosed";
    case COMTRACE_DDE_EVENT_TRANSACTION_STARTED:
        return L"DdeTransactionStarted";
    case COMTRACE_DDE_EVENT_TRANSACTION_COMPLETED:
        return L"DdeTransactionCompleted";
    case COMTRACE_DDE_EVENT_CALLBACK_RECEIVED:
        return L"DdeCallbackReceived";
    default:
        return L"Unknown";
    }
}
