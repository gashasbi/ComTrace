#ifndef COMTRACE_ETL_INSPECT_H
#define COMTRACE_ETL_INSPECT_H

#include <wchar.h>

int comtrace_inspect_etl(
    const wchar_t *etl_path,
    const wchar_t *limit_text
);

#endif