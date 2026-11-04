#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include "etl_archive.h"
#include "archive_reader.h"

static void print_usage(void)
{
    wprintf(L"Usage: ComTraceArchive <input.etl> <output.ctra>\n");
    wprintf(L"       ComTraceArchive --inspect <input.ctra>\n");
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

    if (argc != 3) {
        print_usage();
        return EXIT_FAILURE;
    }

    return comtrace_archive_etl(argv[1], argv[2]);
}
