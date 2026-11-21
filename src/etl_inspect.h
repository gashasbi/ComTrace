#ifndef COMTRACE_ETL_INSPECT_H
#define COMTRACE_ETL_INSPECT_H

#include <windows.h>
#include <wchar.h>

typedef struct {
    BOOL has_provider_id;
    GUID provider_id;
    BOOL has_event_id;
    USHORT event_id;
    BOOL has_process_id;
    ULONG process_id;
    BOOL has_thread_id;
    ULONG thread_id;
    BOOL com_only;
    BOOL ole_only;
    ULONG sample_limit;
} COMTRACE_ETL_FILTERS;


int comtrace_inspect_etl(
    const wchar_t *etl_path,
    const wchar_t *limit_text
);

int comtrace_search_etl(
    const wchar_t *etl_path,
    const COMTRACE_ETL_FILTERS *filters
);

#endif