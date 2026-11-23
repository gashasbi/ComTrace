#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <psapi.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "archive_writer.h"
#include "archive_reader.h"

#define STRESS_EVENT_COUNT 100000ULL
#define STRESS_PAYLOAD_SIZE 512U

static int create_test_path(wchar_t *path, size_t capacity)
{
    wchar_t directory[MAX_PATH];
    wchar_t temporary[MAX_PATH];
    DWORD length;
    int written;

    length = GetTempPathW(MAX_PATH, directory);
    if (length == 0U || length >= MAX_PATH) {
        return 0;
    }

    if (GetTempFileNameW(directory, L"cts", 0U, temporary) == 0U) {
        return 0;
    }

    if (!DeleteFileW(temporary)) {
        return 0;
    }

    written = swprintf_s(path, capacity, L"%ls.ctra", temporary);
    return written > 0;
}

static void fill_payload(BYTE *payload, ULONGLONG sequence)
{
    ULONG index;

    CopyMemory(payload, &sequence, sizeof(sequence));
    for (index = (ULONG)sizeof(sequence); index < STRESS_PAYLOAD_SIZE; ++index) {
        payload[index] = (BYTE)((sequence + index) & 0xFFU);
    }
}

static int validate_payload(const BYTE *payload, ULONGLONG sequence)
{
    BYTE expected[STRESS_PAYLOAD_SIZE];
    fill_payload(expected, sequence);
    return memcmp(payload, expected, sizeof(expected)) == 0;
}

static double elapsed_seconds(
    LARGE_INTEGER start,
    LARGE_INTEGER end,
    LARGE_INTEGER frequency
)
{
    return (double)(end.QuadPart - start.QuadPart) /
        (double)frequency.QuadPart;
}

static ULONGLONG filetime_value(FILETIME value)
{
    ULARGE_INTEGER result;

    result.LowPart = value.dwLowDateTime;
    result.HighPart = value.dwHighDateTime;
    return result.QuadPart;
}

int main(int argc, char **argv)
{
    COMTRACE_ARCHIVE_WRITER writer;
    COMTRACE_ARCHIVE_READER reader;
    COMTRACE_ARCHIVE_EVENT event;
    COMTRACE_ARCHIVE_SUMMARY summary;
    COMTRACE_ARCHIVE_STATUS writer_status;
    COMTRACE_ARCHIVE_READER_STATUS reader_status;
    EVENT_RECORD source;
    BYTE payload[STRESS_PAYLOAD_SIZE];
    wchar_t path[MAX_PATH + 16U];
    LARGE_INTEGER frequency;
    LARGE_INTEGER write_start;
    LARGE_INTEGER write_end;
    LARGE_INTEGER read_start;
    LARGE_INTEGER read_end;
    PROCESS_MEMORY_COUNTERS_EX memory;
    FILETIME process_creation;
    FILETIME process_exit;
    FILETIME kernel_time;
    FILETIME user_time;
    HANDLE file;
    LARGE_INTEGER file_size;
    ULONGLONG index;
    ULONGLONG event_count = STRESS_EVENT_COUNT;
    ULONGLONG records_read = 0ULL;
    ULONGLONG cpu_start = 0ULL;
    ULONGLONG cpu_end = 0ULL;
    int require_large_file = 0;
    DWORD failures = 0U;
    int result = EXIT_FAILURE;
    double write_seconds;
    double read_seconds;
    double cpu_seconds;

    ZeroMemory(&writer, sizeof(writer));
    ZeroMemory(&reader, sizeof(reader));
    ZeroMemory(&event, sizeof(event));
    ZeroMemory(&summary, sizeof(summary));
    ZeroMemory(&source, sizeof(source));
    ZeroMemory(&memory, sizeof(memory));

    if (argc > 2) {
        fputs("Usage: ComTraceArchiveStressTests [event-count|--large].\n", stderr);
        return EXIT_FAILURE;
    }

    if (argc == 2) {
        if (strcmp(argv[1], "--large") == 0) {
            event_count = 9000000ULL;
            require_large_file = 1;
        } else {
            char *end = NULL;
            const char *digit;
            unsigned long long parsed;

            for (digit = argv[1]; *digit != '\0'; ++digit) {
                if (*digit < '0' || *digit > '9') {
                    fputs("Invalid stress event count.\n", stderr);
                    return EXIT_FAILURE;
                }
            }

            errno = 0;
            parsed = strtoull(argv[1], &end, 10);
            if (errno == ERANGE || argv[1][0] == '\0' ||
                end == argv[1] || *end != '\0' || parsed == 0ULL) {
                fputs("Invalid stress event count.\n", stderr);
                return EXIT_FAILURE;
            }

            event_count = (ULONGLONG)parsed;
        }
    }

    if (!create_test_path(path, _countof(path)) ||
        !QueryPerformanceFrequency(&frequency) ||
        frequency.QuadPart <= 0) {
        fputs("Unable to initialize archive stress test.\n", stderr);
        return EXIT_FAILURE;
    }

    writer_status = comtrace_archive_writer_open(&writer, path);
    if (writer_status != COMTRACE_ARCHIVE_SUCCESS) {
        fputs("Archive stress writer open failed.\n", stderr);
        goto cleanup;
    }

    source.EventHeader.Size = (USHORT)sizeof(EVENT_HEADER);
    source.EventHeader.ProviderId.Data1 = 0x12345678UL;
    source.EventHeader.ProviderId.Data2 = 0x9ABC;
    source.EventHeader.ProviderId.Data3 = 0xDEF0;
    source.EventHeader.ProviderId.Data4[0] = 0x12U;
    source.EventHeader.ProviderId.Data4[1] = 0x34U;
    source.EventHeader.ProviderId.Data4[2] = 0x56U;
    source.EventHeader.ProviderId.Data4[3] = 0x78U;
    source.EventHeader.ProviderId.Data4[4] = 0x9AU;
    source.EventHeader.ProviderId.Data4[5] = 0xBCU;
    source.EventHeader.ProviderId.Data4[6] = 0xDEU;
    source.EventHeader.ProviderId.Data4[7] = 0xF0U;
    source.EventHeader.EventDescriptor.Version = 1U;
    source.EventHeader.EventDescriptor.Opcode = 1U;
    source.BufferContext.ProcessorNumber = 1U;
    source.UserData = payload;
    source.UserDataLength = (USHORT)sizeof(payload);

    summary.process_trace_status = ERROR_SUCCESS;
    summary.trace_clock_type = 1U;
    summary.pointer_size = (ULONG)sizeof(void *);
    summary.buffers_read = (ULONG)(event_count / 1000ULL);
    summary.buffers_written = summary.buffers_read;
    summary.perf_frequency = 10000000ULL;

    if (GetProcessTimes(
            GetCurrentProcess(),
            &process_creation,
            &process_exit,
            &kernel_time,
            &user_time)) {
        cpu_start = filetime_value(kernel_time) + filetime_value(user_time);
    }

    (void)QueryPerformanceCounter(&write_start);
    for (index = 0ULL; index < event_count; ++index) {
        fill_payload(payload, index);
        source.EventHeader.ProcessId = (ULONG)(1000ULL + (index % 16ULL));
        source.EventHeader.ThreadId = (ULONG)(2000ULL + (index % 64ULL));
        source.EventHeader.TimeStamp.QuadPart = (LONGLONG)(index + 1ULL);
        source.EventHeader.EventDescriptor.Id = (USHORT)(index % 64ULL);

        writer_status = comtrace_archive_writer_write_event(&writer, &source);
        if (writer_status != COMTRACE_ARCHIVE_SUCCESS) {
            fprintf(stderr, "Archive write failed at event %llu.\n",
                (unsigned long long)index);
            ++failures;
            goto cleanup;
        }
    }
    (void)QueryPerformanceCounter(&write_end);

    writer_status = comtrace_archive_writer_finalize(&writer, &summary);
    if (writer_status != COMTRACE_ARCHIVE_SUCCESS) {
        fputs("Archive stress writer finalize failed.\n", stderr);
        ++failures;
        goto cleanup;
    }
    comtrace_archive_writer_destroy(&writer);

    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, NULL,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE || !GetFileSizeEx(file, &file_size)) {
        if (file != INVALID_HANDLE_VALUE) {
            CloseHandle(file);
        }
        fputs("Unable to read stress archive file size.\n", stderr);
        ++failures;
        goto cleanup;
    }
    CloseHandle(file);

    reader_status = comtrace_archive_reader_open(&reader, path);
    if (reader_status != COMTRACE_ARCHIVE_READER_SUCCESS) {
        fputs("Archive stress reader open failed.\n", stderr);
        ++failures;
        goto cleanup;
    }

    (void)QueryPerformanceCounter(&read_start);
    for (;;) {
        reader_status = comtrace_archive_reader_next(&reader, &event);
        if (reader_status == COMTRACE_ARCHIVE_READER_END) {
            break;
        }
        if (reader_status != COMTRACE_ARCHIVE_READER_SUCCESS) {
            fprintf(stderr, "Archive read failed at event %llu with status %u.\n",
                (unsigned long long)records_read,
                (unsigned int)reader_status);
            ++failures;
            goto cleanup;
        }

        if (event.record.UserDataLength != STRESS_PAYLOAD_SIZE ||
            event.record.UserData == NULL ||
            !validate_payload(
                (const BYTE *)event.record.UserData,
                records_read
            )) {
            fprintf(stderr, "Archive payload validation failed at event %llu.\n",
                (unsigned long long)records_read);
            ++failures;
            comtrace_archive_event_destroy(&event);
            goto cleanup;
        }

        ++records_read;
        comtrace_archive_event_destroy(&event);
    }
    (void)QueryPerformanceCounter(&read_end);

    if (records_read != event_count ||
        reader.footer.records_written != event_count ||
        !reader.footer.checksum_present ||
        !reader.footer.checksum_valid ||
        (require_large_file && (ULONGLONG)file_size.QuadPart <= 0x100000000ULL)) {
        fputs("Archive stress count or checksum validation failed.\n", stderr);
        ++failures;
        goto cleanup;
    }

    memory.cb = sizeof(memory);
    if (!GetProcessMemoryInfo(
            GetCurrentProcess(),
            (PROCESS_MEMORY_COUNTERS *)&memory,
            sizeof(memory))) {
        ZeroMemory(&memory, sizeof(memory));
    }

    write_seconds = elapsed_seconds(write_start, write_end, frequency);
    read_seconds = elapsed_seconds(read_start, read_end, frequency);

    if (GetProcessTimes(
            GetCurrentProcess(),
            &process_creation,
            &process_exit,
            &kernel_time,
            &user_time)) {
        cpu_end = filetime_value(kernel_time) + filetime_value(user_time);
    }

    cpu_seconds = cpu_end >= cpu_start
        ? (double)(cpu_end - cpu_start) / 10000000.0
        : 0.0;

    printf(
        "archive_stress events=%llu payload_bytes=%u file_bytes=%llu write_seconds=%.3f read_seconds=%.3f cpu_seconds=%.3f write_events_per_second=%.0f read_events_per_second=%.0f peak_working_set_bytes=%llu checksum=valid\n",
        (unsigned long long)records_read,
        (unsigned int)STRESS_PAYLOAD_SIZE,
        (unsigned long long)file_size.QuadPart,
        write_seconds,
        read_seconds,
        cpu_seconds,
        write_seconds > 0.0 ? (double)records_read / write_seconds : 0.0,
        read_seconds > 0.0 ? (double)records_read / read_seconds : 0.0,
        (unsigned long long)memory.PeakWorkingSetSize
    );

    result = failures == 0U ? EXIT_SUCCESS : EXIT_FAILURE;

cleanup:
    comtrace_archive_event_destroy(&event);
    comtrace_archive_reader_close(&reader);
    comtrace_archive_writer_destroy(&writer);
    (void)DeleteFileW(path);
    if (result != EXIT_SUCCESS || failures != 0U) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
