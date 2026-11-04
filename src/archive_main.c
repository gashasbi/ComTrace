#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include "etl_archive.h"

static void print_usage(void)
{
    wprintf(
        L"Usage: ComTraceArchive <input.etl> <output.ctra>\n"
    );
    wprintf(
        L"Convert all ETL events into a ComTrace binary archive.\n"
    );
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

    if (argc != 3) {
        print_usage();
        return EXIT_FAILURE;
    }

    return comtrace_archive_etl(argv[1], argv[2]);
}