#ifndef COMTRACE_ETL_ARCHIVE_H
#define COMTRACE_ETL_ARCHIVE_H

#include <wchar.h>

int comtrace_archive_etl(
    const wchar_t *etl_path,
    const wchar_t *archive_path
);

#endif