#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tdh.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include "etw_capture.h"
#include "etl_inspect.h"

#define COMTRACE_VERSION "0.1.0"
#define ARRAY_COUNT(values) (sizeof(values) / sizeof((values)[0]))

typedef struct {
    const wchar_t *system_name;
    const wchar_t *display_name;
    const wchar_t *guid;
} COMTRACE_PROVIDER;

static const COMTRACE_PROVIDER known_providers[] = {
    {
        L"Microsoft-Windows-COMRuntime",
        L"Microsoft-Windows-COMRuntime",
        L"{BF406804-6AFA-46E7-8A48-6C357E1D6D61}"
    },
    {
        L"Microsoft-Windows-COM-Perf",
        L"Microsoft-Windows-COM-Perf",
        L"{B8D6861B-D20F-4EEC-BBAE-87E0DD80602B}"
    },
    {
        L"Microsoft-Windows-COM-RundownInstrumentation",
        L"Microsoft-Windows-COM-RundownInstrumentation",
        L"{2957313D-FCAA-5D4A-2F69-32CE5F0AC44E}"
    }
};

static void print_usage(const wchar_t *program_name)
{
    wprintf(
        L"Usage: %ls [--help | --version | --self-test | --providers | --capture <seconds> <output.etl> | --inspect <file.etl> [max-events]]\n",
        program_name
    );
}

static int run_self_test(void)
{
    size_t count = ARRAY_COUNT(known_providers);
    size_t index;

    if (count != (size_t)3) {
        fwprintf(stderr, L"Provider catalog size check failed.\n");
        return EXIT_FAILURE;
    }

    for (index = 0U; index < count; ++index) {
        const COMTRACE_PROVIDER *provider = &known_providers[index];
        size_t guid_length;

        if (provider->system_name == NULL ||
            provider->display_name == NULL ||
            provider->guid == NULL) {
            fwprintf(stderr, L"Provider catalog contains a null field.\n");
            return EXIT_FAILURE;
        }

        if (provider->system_name[0] == L'\0' ||
            provider->display_name[0] == L'\0') {
            fwprintf(stderr, L"Provider catalog contains an empty name.\n");
            return EXIT_FAILURE;
        }

        guid_length = wcslen(provider->guid);
        if (guid_length != (size_t)38 ||
            provider->guid[0] != L'{' ||
            provider->guid[guid_length - 1U] != L'}') {
            fwprintf(stderr, L"Provider catalog contains an invalid GUID string.\n");
            return EXIT_FAILURE;
        }
    }

    wprintf(L"ComTrace self-test: PASS\n");
    return EXIT_SUCCESS;
}

static int list_providers(void)
{
    ULONG buffer_size = 0U;
    ULONG status = ERROR_SUCCESS;
    unsigned int attempt;
    PPROVIDER_ENUMERATION_INFO providers = NULL;
    size_t found_count = 0U;
    size_t catalog_index;

    for (attempt = 0U; attempt < 8U; ++attempt) {
        PPROVIDER_ENUMERATION_INFO resized;

        status = TdhEnumerateProviders(providers, &buffer_size);
        if (status != ERROR_INSUFFICIENT_BUFFER) {
            break;
        }

        if (buffer_size == 0U) {
            status = ERROR_INVALID_DATA;
            break;
        }

        if (providers == NULL) {
            resized = (PPROVIDER_ENUMERATION_INFO)HeapAlloc(
                GetProcessHeap(),
                HEAP_ZERO_MEMORY,
                (SIZE_T)buffer_size
            );
        } else {
            resized = (PPROVIDER_ENUMERATION_INFO)HeapReAlloc(
                GetProcessHeap(),
                HEAP_ZERO_MEMORY,
                providers,
                (SIZE_T)buffer_size
            );
        }

        if (resized == NULL) {
            if (providers != NULL) {
                (void)HeapFree(GetProcessHeap(), 0U, providers);
            }

            fwprintf(stderr, L"Unable to allocate provider inventory buffer.\n");
            return EXIT_FAILURE;
        }

        providers = resized;
    }

    if (status != ERROR_SUCCESS || providers == NULL) {
        fwprintf(
            stderr,
            L"TdhEnumerateProviders failed with status %lu.\n",
            (unsigned long)status
        );

        if (providers != NULL) {
            (void)HeapFree(GetProcessHeap(), 0U, providers);
        }

        return EXIT_FAILURE;
    }

    wprintf(L"ComTrace %hs\n", COMTRACE_VERSION);
    wprintf(L"COM/OLE ETW provider inventory\n");
    wprintf(L"Source: Windows Trace Data Helper\n\n");

    for (catalog_index = 0U;
         catalog_index < ARRAY_COUNT(known_providers);
         ++catalog_index) {
        const COMTRACE_PROVIDER *known = &known_providers[catalog_index];
        int registered = 0;
        ULONG provider_index;

        for (provider_index = 0U;
             provider_index < providers->NumberOfProviders;
             ++provider_index) {
            const TRACE_PROVIDER_INFO *entry =
                &providers->TraceProviderInfoArray[provider_index];
            const wchar_t *actual_name;
            size_t remaining_chars;
            size_t scanned_chars = 0U;

            if (entry->ProviderNameOffset >= buffer_size) {
                continue;
            }

            if (((size_t)entry->ProviderNameOffset % sizeof(wchar_t)) !=
                (size_t)0) {
                continue;
            }

            actual_name = (const wchar_t *)(
                (const BYTE *)providers + entry->ProviderNameOffset
            );

            remaining_chars =
                ((size_t)buffer_size - (size_t)entry->ProviderNameOffset) /
                sizeof(wchar_t);

            while (scanned_chars < remaining_chars &&
                   actual_name[scanned_chars] != L'\0') {
                ++scanned_chars;
            }

            if (scanned_chars == remaining_chars) {
                continue;
            }

            if (_wcsicmp(actual_name, known->system_name) == 0) {
                registered = 1;
                break;
            }
        }

        wprintf(
            L"%ls | %ls | %ls\n",
            known->display_name,
            known->guid,
            registered ? L"registered" : L"not registered"
        );

        if (registered) {
            ++found_count;
        }
    }

    wprintf(
        L"\nFound %zu of %zu known COM ETW providers.\n",
        found_count,
        ARRAY_COUNT(known_providers)
    );
    wprintf(
        L"Registration does not establish that events were emitted or captured.\n"
    );

    (void)HeapFree(GetProcessHeap(), 0U, providers);
    return EXIT_SUCCESS;
}

int wmain(int argc, wchar_t **argv)
{
    if (argc == 1) {
        wprintf(L"ComTrace %hs\n", COMTRACE_VERSION);
        wprintf(L"Native Windows COM/OLE tracing toolkit\n");
        wprintf(L"Use --providers to inspect known COM ETW provider registrations.\n");
        wprintf(L"Use --capture <seconds> <output.etl> to record a trace.\n");
        return EXIT_SUCCESS;
    }

    if ((argc == 3 || argc == 4) && wcscmp(argv[1], L"--inspect") == 0) {
        return comtrace_inspect_etl(argv[2], argc == 4 ? argv[3] : L"100");
    }
    if (argc == 4 && wcscmp(argv[1], L"--capture") == 0) {
        return comtrace_capture_com_perf(argv[2], argv[3]);
    }

    if (argc == 2) {
        if (wcscmp(argv[1], L"--help") == 0) {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        }

        if (wcscmp(argv[1], L"--version") == 0) {
            wprintf(L"ComTrace %hs\n", COMTRACE_VERSION);
            return EXIT_SUCCESS;
        }

        if (wcscmp(argv[1], L"--self-test") == 0) {
            return run_self_test();
        }

        if (wcscmp(argv[1], L"--providers") == 0) {
            return list_providers();
        }
    }

    print_usage(argv[0]);
    return EXIT_FAILURE;
}