#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "dde_event.h"

static const GUID provider_id = {
    0x7b6d4a21,
    0x9e35,
    0x4c80,
    {0xa1, 0x72, 0x53, 0x94, 0xb6, 0xd8, 0x0f, 0x21}
};

static const GUID other_provider_id = {
    0x7b6d4a22,
    0x9e35,
    0x4c80,
    {0xa1, 0x72, 0x53, 0x94, 0xb6, 0xd8, 0x0f, 0x21}
};

int main(void)
{
    COMTRACE_DDE_EVENT_TYPE type = COMTRACE_DDE_EVENT_UNKNOWN;

    if (!comtrace_is_dde_provider(&provider_id) ||
        comtrace_is_dde_provider(&other_provider_id) ||
        !comtrace_classify_dde_event(
            &provider_id,
            L"DdeTransactionStarted",
            &type
        ) ||
        type != COMTRACE_DDE_EVENT_TRANSACTION_STARTED ||
        wcscmp(
            comtrace_dde_event_name(type),
            L"DdeTransactionStarted"
        ) != 0 ||
        comtrace_classify_dde_event(
            &provider_id,
            L"UnknownDdeEvent",
            &type
        ) ||
        comtrace_classify_dde_event(
            &other_provider_id,
            L"DdeConversationOpened",
            &type
        ) ||
        comtrace_classify_dde_event(&provider_id, NULL, &type) ||
        comtrace_classify_dde_event(&provider_id, L"DdeConversationOpened", NULL)) {
        fputs("DDE event tests failed.\n", stderr);
        return 1;
    }

    puts("DDE event tests passed.");
    return 0;
}
