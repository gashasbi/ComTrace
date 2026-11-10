#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include "etl_archive.h"
#include "archive_reader.h"

static void print_usage(void)
{
    wprintf(L"Usage: ComTraceArchive <input.etl> <output.ctra>\n");
    wprintf(L"       ComTraceArchive --inspect <input.ctra>\n");
    wprintf(L"       ComTraceArchive --recover <input.ctra>\n");
    wprintf(L"       ComTraceArchive --help\n");
    wprintf(L"       ComTraceArchive --version\n");
}

static int inspect_archive(const wchar_t *path)
{
    COMTRACE_ARCHIVE_READER reader;
    COMTRACE_ARCHIVE_EVENT event;
    COMTRACE_ARCHIVE_READER_STATUS status;
    ULONGLONG events_read = 0ULL;

    ZeroMemory(&reader, sizeof(reader));
    ZeroMemory(&event, sizeof(event));

    status = comtrace_archive_reader_open(&reader, path);
    if (status != COMTRACE_ARCHIVE_READER_SUCCESS) {
        fwprintf(
            stderr,
            L"Archive open failed. Status: %u Win32 error: %lu.\n",
            (unsigned int)status,
            (unsigned long)reader.win32_error
        );
        comtrace_archive_reader_close(&reader);
        return EXIT_FAILURE;
    }

    for (;;) {
        status = comtrace_archive_reader_next(&reader, &event);
        if (status == COMTRACE_ARCHIVE_READER_END) {
            break;
        }

        if (status != COMTRACE_ARCHIVE_READER_SUCCESS) {
            fwprintf(
                stderr,
                L"Archive read failed. Status: %u Win32 error: %lu.\n",
                (unsigned int)status,
                (unsigned long)reader.win32_error
            );
            comtrace_archive_event_destroy(&event);
            comtrace_archive_reader_close(&reader);
            return EXIT_FAILURE;
        }

        if (events_read == UINT64_MAX) {
            fwprintf(stderr, L"Archive event count overflow.\n");
            comtrace_archive_event_destroy(&event);
            comtrace_archive_reader_close(&reader);
            return EXIT_FAILURE;
        }

        ++events_read;
        comtrace_archive_event_destroy(&event);
    }

    wprintf(L"ComTraceArchive 0.1.0\n");
    wprintf(L"Archive: %ls\n\n", path);
    wprintf(L"Archive validation: PASS\n");
    wprintf(L"Events read: %llu\n", (unsigned long long)events_read);
    wprintf(L"Events lost: %lu\n", (unsigned long)reader.footer.events_lost);
    wprintf(L"Buffers read: %lu\n", (unsigned long)reader.footer.buffers_read);
    wprintf(L"Buffers written: %lu\n", (unsigned long)reader.footer.buffers_written);
    wprintf(L"Buffers lost: %lu\n", (unsigned long)reader.footer.buffers_lost);
    wprintf(L"Trace clock type: %lu\n", (unsigned long)reader.footer.trace_clock_type);
    wprintf(L"Pointer size: %lu\n", (unsigned long)reader.footer.pointer_size);

    comtrace_archive_reader_close(&reader);
    return EXIT_SUCCESS;
}


static const wchar_t *recovery_stop_reason_name(
    COMTRACE_ARCHIVE_RECOVERY_STOP_REASON reason
)
{
    switch (reason) {
    case COMTRACE_ARCHIVE_RECOVERY_STOP_NONE:
        return L"none";
    case COMTRACE_ARCHIVE_RECOVERY_STOP_FOOTER_MISSING:
        return L"footer missing";
    case COMTRACE_ARCHIVE_RECOVERY_STOP_FOOTER_TRUNCATED:
        return L"footer truncated";
    case COMTRACE_ARCHIVE_RECOVERY_STOP_FOOTER_INVALID:
        return L"footer invalid";
    case COMTRACE_ARCHIVE_RECOVERY_STOP_RECORD_HEADER_TRUNCATED:
        return L"record header truncated";
    case COMTRACE_ARCHIVE_RECOVERY_STOP_RECORD_PAYLOAD_TRUNCATED:
        return L"record payload truncated";
    case COMTRACE_ARCHIVE_RECOVERY_STOP_RECORD_INVALID:
        return L"record invalid";
    case COMTRACE_ARCHIVE_RECOVERY_STOP_TRAILING_DATA:
        return L"trailing data";
    default:
        return L"unknown";
    }
}

static int recover_archive(const wchar_t *path)
{
    COMTRACE_ARCHIVE_READER reader;
    COMTRACE_ARCHIVE_EVENT event;
    COMTRACE_ARCHIVE_READER_STATUS status;
    const COMTRACE_ARCHIVE_RECOVERY_INFO *info;

    ZeroMemory(&reader, sizeof(reader));
    ZeroMemory(&event, sizeof(event));

    status = comtrace_archive_reader_open_recovery(&reader, path);
    if (status != COMTRACE_ARCHIVE_READER_SUCCESS) {
        fwprintf(
            stderr,
            L"Archive recovery open failed. Status: %u Win32 error: %lu.\n",
            (unsigned int)status,
            (unsigned long)reader.win32_error
        );
        comtrace_archive_reader_close(&reader);
        return EXIT_FAILURE;
    }

    for (;;) {
        status = comtrace_archive_reader_next(&reader, &event);
        if (status == COMTRACE_ARCHIVE_READER_END) {
            break;
        }

        if (status != COMTRACE_ARCHIVE_READER_SUCCESS) {
            fwprintf(
                stderr,
                L"Archive recovery read failed. Status: %u Win32 error: %lu.\n",
                (unsigned int)status,
                (unsigned long)reader.win32_error
            );
            comtrace_archive_event_destroy(&event);
            comtrace_archive_reader_close(&reader);
            return EXIT_FAILURE;
        }

        comtrace_archive_event_destroy(&event);
    }

    info = comtrace_archive_reader_get_recovery_info(&reader);
    if (info == NULL) {
        fwprintf(stderr, L"Archive recovery information is unavailable.\n");
        comtrace_archive_reader_close(&reader);
        return EXIT_FAILURE;
    }

    wprintf(L"ComTraceArchive 0.1.0\n");
    wprintf(L"Archive: %ls\n\n", path);
    wprintf(
        L"Records recovered: %llu\n",
        (unsigned long long)info->records_recovered
    );
    wprintf(
        L"Stop offset: %llu\n",
        (unsigned long long)info->stop_offset
    );
    wprintf(
        L"Stop reason: %ls\n",
        recovery_stop_reason_name(info->stop_reason)
    );

    comtrace_archive_reader_close(&reader);
    return EXIT_SUCCESS;
}
int wmain(int argc, wchar_t **argv)
{
    if (argc == 2 && wcscmp(argv[1], L"--help") == 0) {
        print_usage();
        return EXIT_SUCCESS;
    }

    if (argc == 2 && wcscmp(argv[1], L"--version") == 0) {
        wprintf(L"ComTraceArchive 0.1.0\n");
        return EXIT_SUCCESS;
    }

    if (argc == 3 && wcscmp(argv[1], L"--inspect") == 0) {
        return inspect_archive(argv[2]);
    }

    if (argc == 3 && wcscmp(argv[1], L"--recover") == 0) {
        return recover_archive(argv[2]);
    }

    if (argc != 3) {
        print_usage();
        return EXIT_FAILURE;
    }

    return comtrace_archive_etl(argv[1], argv[2]);
}
