#ifndef COMTRACE_COM_EVENT_H
#define COMTRACE_COM_EVENT_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

int comtrace_classify_com_event(
    const GUID *provider_id,
    USHORT event_id,
    UCHAR version,
    UCHAR opcode,
    const wchar_t **operation,
    const wchar_t **phase
);

#endif
