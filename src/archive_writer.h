#ifndef COMTRACE_ARCHIVE_WRITER_H
#define COMTRACE_ARCHIVE_WRITER_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <evntcons.h>

typedef enum {
    COMTRACE_ARCHIVE_SUCCESS = 0,
    COMTRACE_ARCHIVE_INVALID_ARGUMENT,
    COMTRACE_ARCHIVE_OUTPUT_EXISTS,
    COMTRACE_ARCHIVE_OPEN_FAILED,
    COMTRACE_ARCHIVE_INVALID_EVENT,
    COMTRACE_ARCHIVE_ALLOCATION_FAILED,
    COMTRACE_ARCHIVE_SIZE_OVERFLOW,
    COMTRACE_ARCHIVE_WRITE_FAILED,
    COMTRACE_ARCHIVE_FINALIZE_FAILED
} COMTRACE_ARCHIVE_STATUS;

typedef struct {
    ULONG events_lost;
    ULONG buffers_lost;
    ULONG buffers_written;
    ULONG buffers_read;
    ULONG process_trace_status;
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
} COMTRACE_ARCHIVE_SUMMARY;

typedef struct {
    HANDLE file;
    wchar_t *final_path;
    wchar_t *partial_path;
    ULONGLONG records_written;
    DWORD win32_error;
    COMTRACE_ARCHIVE_STATUS status;
} COMTRACE_ARCHIVE_WRITER;

COMTRACE_ARCHIVE_STATUS comtrace_archive_writer_open(
    COMTRACE_ARCHIVE_WRITER *writer,
    const wchar_t *output_path
);

COMTRACE_ARCHIVE_STATUS comtrace_archive_writer_write_event(
    COMTRACE_ARCHIVE_WRITER *writer,
    const EVENT_RECORD *event_record
);

COMTRACE_ARCHIVE_STATUS comtrace_archive_writer_finalize(
    COMTRACE_ARCHIVE_WRITER *writer,
    const COMTRACE_ARCHIVE_SUMMARY *summary
);

void comtrace_archive_writer_abort(
    COMTRACE_ARCHIVE_WRITER *writer
);

void comtrace_archive_writer_destroy(
    COMTRACE_ARCHIVE_WRITER *writer
);

const wchar_t *comtrace_archive_writer_partial_path(
    const COMTRACE_ARCHIVE_WRITER *writer
);

#endif