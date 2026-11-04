#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <evntrace.h>
#include <evntcons.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "etl_archive.h"
#include "archive_writer.h"

typedef struct {
    COMTRACE_ARCHIVE_WRITER *writer;
    ULONGLONG events_seen;
    ULONG buffers_read;
    COMTRACE_ARCHIVE_STATUS write_status;
} ETL_ARCHIVE_CONTEXT;

static VOID WINAPI archive_event_callback(PEVENT_RECORD event_record)
{
    ETL_ARCHIVE_CONTEXT *context;

    if (event_record == NULL || event_record->UserContext == NULL) {
        return;
    }

    context = (ETL_ARCHIVE_CONTEXT *)event_record->UserContext;

    if (context->events_seen == ULLONG_MAX) {
        context->write_status = COMTRACE_ARCHIVE_SIZE_OVERFLOW;
        return;
    }

    ++context->events_seen;

    if (context->write_status != COMTRACE_ARCHIVE_SUCCESS) {
        return;
    }

    context->write_status = comtrace_archive_writer_write_event(
        context->writer,
        event_record
    );
}

static ULONG WINAPI archive_buffer_callback(
    PEVENT_TRACE_LOGFILEW logfile
)
{
    ETL_ARCHIVE_CONTEXT *context;

    if (logfile == NULL || logfile->Context == NULL) {
        return FALSE;
    }

    context = (ETL_ARCHIVE_CONTEXT *)logfile->Context;

    if (context->buffers_read == ULONG_MAX) {
        context->write_status = COMTRACE_ARCHIVE_SIZE_OVERFLOW;
        return FALSE;
    }

    ++context->buffers_read;
    return TRUE;
}
static void populate_archive_summary(
    const EVENT_TRACE_LOGFILEW *logfile,
    const ETL_ARCHIVE_CONTEXT *context,
    ULONG process_status,
    COMTRACE_ARCHIVE_SUMMARY *summary
)
{
    const TRACE_LOGFILE_HEADER *header = &logfile->LogfileHeader;

    ZeroMemory(summary, sizeof(*summary));

    summary->events_lost = header->EventsLost;
    summary->buffers_lost = header->BuffersLost;
    summary->buffers_written = header->BuffersWritten;
    summary->buffers_read = context->buffers_read;
    summary->process_trace_status = process_status;
    summary->trace_clock_type = header->ReservedFlags;
    summary->pointer_size = header->PointerSize;
    summary->os_version = header->Version;
    summary->provider_version = header->ProviderVersion;
    summary->number_of_processors = header->NumberOfProcessors;
    summary->buffer_size = header->BufferSize;
    summary->timer_resolution = header->TimerResolution;
    summary->maximum_file_size = header->MaximumFileSize;
    summary->log_file_mode = header->LogFileMode;
    summary->start_buffers = header->StartBuffers;
    summary->cpu_speed_mhz = header->CpuSpeedInMHz;
    summary->start_time = (ULONGLONG)header->StartTime.QuadPart;
    summary->end_time = (ULONGLONG)header->EndTime.QuadPart;
    summary->boot_time = (ULONGLONG)header->BootTime.QuadPart;
    summary->perf_frequency = (ULONGLONG)header->PerfFreq.QuadPart;
}

static void print_partial_archive(
    const COMTRACE_ARCHIVE_WRITER *writer
)
{
    const wchar_t *path;

    path = comtrace_archive_writer_partial_path(writer);

    if (path != NULL &&
        GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES) {
        fwprintf(stderr, L"Partial archive retained: %ls\n", path);
    }
}

int comtrace_archive_etl(
    const wchar_t *etl_path,
    const wchar_t *archive_path
)
{
    ETL_ARCHIVE_CONTEXT context;
    EVENT_TRACE_LOGFILEW logfile;
    COMTRACE_ARCHIVE_WRITER writer;
    COMTRACE_ARCHIVE_SUMMARY summary;
    COMTRACE_ARCHIVE_STATUS archive_status;
    TRACEHANDLE trace_handle;
    ULONG process_status;
    ULONG close_status;
    DWORD attributes;
    int result = EXIT_FAILURE;

    ZeroMemory(&context, sizeof(context));
    ZeroMemory(&logfile, sizeof(logfile));
    ZeroMemory(&writer, sizeof(writer));
    ZeroMemory(&summary, sizeof(summary));

    writer.file = INVALID_HANDLE_VALUE;

    if (etl_path == NULL || etl_path[0] == L'\0' ||
        archive_path == NULL || archive_path[0] == L'\0') {
        fwprintf(stderr, L"Both ETL input and archive output paths are required.\n");
        return EXIT_FAILURE;
    }

    attributes = GetFileAttributesW(etl_path);

    if (attributes == INVALID_FILE_ATTRIBUTES) {
        fwprintf(
            stderr,
            L"Unable to access ETL input path. Win32 error: %lu\n",
            (unsigned long)GetLastError()
        );
        return EXIT_FAILURE;
    }

    if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0U) {
        fwprintf(stderr, L"ETL input path is a directory.\n");
        return EXIT_FAILURE;
    }

    logfile.LogFileName = (LPWSTR)etl_path;
    logfile.ProcessTraceMode =
        PROCESS_TRACE_MODE_EVENT_RECORD |
        PROCESS_TRACE_MODE_RAW_TIMESTAMP;
    logfile.EventRecordCallback = archive_event_callback;
    logfile.BufferCallback = archive_buffer_callback;
    logfile.Context = &context;

    trace_handle = OpenTraceW(&logfile);

    if (trace_handle == INVALID_PROCESSTRACE_HANDLE) {
        fwprintf(
            stderr,
            L"OpenTraceW failed with Win32 error %lu.\n",
            (unsigned long)GetLastError()
        );
        return EXIT_FAILURE;
    }

    archive_status = comtrace_archive_writer_open(
        &writer,
        archive_path
    );

    if (archive_status != COMTRACE_ARCHIVE_SUCCESS) {
        fwprintf(
            stderr,
            L"Archive writer open failed. Status: %u Win32 error: %lu.\n",
            (unsigned int)archive_status,
            (unsigned long)writer.win32_error
        );

        close_status = CloseTrace(trace_handle);

        if (close_status != ERROR_SUCCESS) {
            fwprintf(
                stderr,
                L"CloseTrace failed with status %lu.\n",
                (unsigned long)close_status
            );
        }

        comtrace_archive_writer_destroy(&writer);
        return EXIT_FAILURE;
    }

    context.writer = &writer;
    context.write_status = COMTRACE_ARCHIVE_SUCCESS;

    wprintf(L"ComTraceArchive 0.1.0\n");
    wprintf(L"Input: %ls\n", etl_path);
    wprintf(L"Output: %ls\n\n", archive_path);

    process_status = ProcessTrace(
        &trace_handle,
        1U,
        NULL,
        NULL
    );

    close_status = CloseTrace(trace_handle);

    populate_archive_summary(
        &logfile,
        &context,
        process_status,
        &summary
    );

    if (process_status != ERROR_SUCCESS) {
        fwprintf(
            stderr,
            L"ProcessTrace failed with status %lu.\n",
            (unsigned long)process_status
        );
    }

    if (close_status != ERROR_SUCCESS) {
        fwprintf(
            stderr,
            L"CloseTrace failed with status %lu.\n",
            (unsigned long)close_status
        );
    }

    if (context.write_status != COMTRACE_ARCHIVE_SUCCESS) {
        fwprintf(
            stderr,
            L"Event archive write failed. Status: %u Win32 error: %lu.\n",
            (unsigned int)context.write_status,
            (unsigned long)writer.win32_error
        );
    }

    if (process_status != ERROR_SUCCESS ||
        close_status != ERROR_SUCCESS ||
        context.write_status != COMTRACE_ARCHIVE_SUCCESS) {
        comtrace_archive_writer_abort(&writer);
        print_partial_archive(&writer);
        comtrace_archive_writer_destroy(&writer);
        return EXIT_FAILURE;
    }

    archive_status = comtrace_archive_writer_finalize(
        &writer,
        &summary
    );

    if (archive_status != COMTRACE_ARCHIVE_SUCCESS) {
        fwprintf(
            stderr,
            L"Archive finalization failed. Status: %u Win32 error: %lu.\n",
            (unsigned int)archive_status,
            (unsigned long)writer.win32_error
        );

        comtrace_archive_writer_abort(&writer);
        print_partial_archive(&writer);
        comtrace_archive_writer_destroy(&writer);
        return EXIT_FAILURE;
    }

    wprintf(L"\nArchive summary\n");
    wprintf(
        L"Events archived: %llu\n",
        (unsigned long long)writer.records_written
    );
    wprintf(
        L"Events lost: %lu\n",
        (unsigned long)summary.events_lost
    );
    wprintf(
        L"Buffers read: %lu\n",
        (unsigned long)summary.buffers_read
    );
    wprintf(
        L"Buffers written: %lu\n",
        (unsigned long)summary.buffers_written
    );
    wprintf(
        L"Buffers lost: %lu\n",
        (unsigned long)summary.buffers_lost
    );
    wprintf(
        L"Trace clock type: %lu\n",
        (unsigned long)summary.trace_clock_type
    );
    wprintf(
        L"Pointer size: %lu\n",
        (unsigned long)summary.pointer_size
    );

    result = EXIT_SUCCESS;

    comtrace_archive_writer_destroy(&writer);
    return result;
}