#ifndef COMTRACE_ARCHIVE_CHECKSUM_H
#define COMTRACE_ARCHIVE_CHECKSUM_H

#include <stddef.h>
#include <windows.h>

ULONG comtrace_archive_checksum_begin(void);

ULONG comtrace_archive_checksum_update(
    ULONG state,
    const BYTE *data,
    size_t size
);

ULONG comtrace_archive_checksum_finish(ULONG state);

ULONG comtrace_archive_checksum(
    const BYTE *data,
    size_t size
);

#endif
