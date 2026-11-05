#ifndef COMTRACE_ARCHIVE_READER_H
#define COMTRACE_ARCHIVE_READER_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <evntcons.h>

#include <stdint.h>

typedef enum {
    COMTRACE_ARCHIVE_READER_SUCCESS = 0,
    COMTRACE_ARCHIVE_READER_END,
    COMTRACE_ARCHIVE_READER_INVALID_ARGUMENT,
    COMTRACE_ARCHIVE_READER_OPEN_FAILED,
    COMTRACE_ARCHIVE_READER_INVALID_FORMAT,
    COMTRACE_ARCHIVE_READER_TRUNCATED,
    COMTRACE_ARCHIVE_READER_READ_FAILED,
    COMTRACE_ARCHIVE_READER_ALLOCATION_FAILED
} COMTRACE_ARCHIVE_READER_STATUS;

typedef enum {
    COMTRACE_ARCHIVE_RECOVERY_STOP_NONE = 0,
    COMTRACE_ARCHIVE_RECOVERY_STOP_FOOTER_MISSING,
    COMTRACE_ARCHIVE_RECOVERY_STOP_FOOTER_TRUNCATED,
    COMTRACE_ARCHIVE_RECOVERY_STOP_FOOTER_INVALID,
    COMTRACE_ARCHIVE_RECOVERY_STOP_RECORD_HEADER_TRUNCATED,
    COMTRACE_ARCHIVE_RECOVERY_STOP_RECORD_PAYLOAD_TRUNCATED,
    COMTRACE_ARCHIVE_RECOVERY_STOP_RECORD_INVALID,
    COMTRACE_ARCHIVE_RECOVERY_STOP_TRAILING_DATA
} COMTRACE_ARCHIVE_RECOVERY_STOP_REASON;

typedef struct {
    ULONGLONG records_recovered;
    ULONGLONG stop_offset;
    BOOL footer_valid;
    BOOL archive_complete;
    COMTRACE_ARCHIVE_RECOVERY_STOP_REASON stop_reason;
} COMTRACE_ARCHIVE_RECOVERY_INFO;

typedef struct {
    EVENT_RECORD record;
    EVENT_HEADER_EXTENDED_DATA_ITEM *extended_data;
    BYTE *payload;
} COMTRACE_ARCHIVE_EVENT;

typedef struct {
    ULONGLONG records_written;
    ULONG events_lost;
    ULONG buffers_lost;
    ULONG buffers_written;
    ULONG buffers_read;
    ULONG process_trace_status;
    ULONG completion_marker;
    ULONG trace_clock_type;
    ULONG pointer_size;
    ULONG os_version;
    ULONG provider_version;
    ULONG number_of_processors;
    ULONG buffer_size;
    ULONG timer_resolution;
    ULONG maximum_file_size;
    ULONG log_file_mode;
    ULONG start_buffers;
    ULONG cpu_speed_mhz;
    ULONGLONG start_time;
    ULONGLONG end_time;
    ULONGLONG boot_time;
    ULONGLONG perf_frequency;
} COMTRACE_ARCHIVE_FOOTER;

typedef struct {
    HANDLE file;
    ULONGLONG file_size;
    ULONGLONG offset;
    ULONGLONG records_read;
    BOOL footer_read;
    DWORD win32_error;
    COMTRACE_ARCHIVE_READER_STATUS status;
    COMTRACE_ARCHIVE_FOOTER footer;
    BOOL recovery_mode;
    COMTRACE_ARCHIVE_RECOVERY_INFO recovery_info;
} COMTRACE_ARCHIVE_READER;

COMTRACE_ARCHIVE_READER_STATUS comtrace_archive_reader_open(
    COMTRACE_ARCHIVE_READER *reader,
    const wchar_t *path
);

COMTRACE_ARCHIVE_READER_STATUS comtrace_archive_reader_next(
    COMTRACE_ARCHIVE_READER *reader,
    COMTRACE_ARCHIVE_EVENT *event
);

void comtrace_archive_event_destroy(COMTRACE_ARCHIVE_EVENT *event);
void comtrace_archive_reader_close(COMTRACE_ARCHIVE_READER *reader);

COMTRACE_ARCHIVE_READER_STATUS comtrace_archive_reader_open_recovery(
    COMTRACE_ARCHIVE_READER *reader,
    const wchar_t *path
);

const COMTRACE_ARCHIVE_RECOVERY_INFO *
comtrace_archive_reader_get_recovery_info(
    const COMTRACE_ARCHIVE_READER *reader
);

#endif
