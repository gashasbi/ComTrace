#ifndef COMTRACE_OLE_EVENT_H
#define COMTRACE_OLE_EVENT_H

#include <windows.h>

int comtrace_classify_ole_event(
    const GUID *provider_id,
    USHORT event_id,
    UCHAR version,
    UCHAR opcode,
    const wchar_t **operation,
    const wchar_t **phase
);

#endif
