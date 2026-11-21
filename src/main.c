#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tdh.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <wchar.h>
#include "etw_capture.h"
#include "etl_inspect.h"
#include "trace_compare.h"

#define COMTRACE_VERSION "0.2.0"
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
        L"Usage: %ls [--help | --version | --self-test | --providers | --capture <seconds> <output.etl> | --inspect <file.etl> [max-events] | --compare <left.etl|ctra> <right.etl|ctra> [--json] | --search <file.etl> [--provider <guid>] [--event-id <id>] [--pid <id>] [--tid <id>] [--com | --ole] [--max-events <n>]]\n",
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

static int parse_decimal_value(
    const wchar_t *text,
    ULONG maximum,
    ULONG *value
)
{
    const wchar_t *cursor;
    wchar_t *end = NULL;
    unsigned long parsed;

    if (text == NULL || text[0] == L'\0' || value == NULL) {
        return 0;
    }

    for (cursor = text; *cursor != L'\0'; ++cursor) {
        if (*cursor < L'0' || *cursor > L'9') {
            return 0;
        }
    }

    errno = 0;
    parsed = wcstoul(text, &end, 10);
    if (errno == ERANGE || end == text || end == NULL ||
        *end != L'\0' || parsed > maximum) {
        return 0;
    }

    *value = (ULONG)parsed;
    return 1;
}

static int parse_hex_digit(wchar_t character)
{
    if (character >= L'0' && character <= L'9') {
        return (int)(character - L'0');
    }
    if (character >= L'a' && character <= L'f') {
        return (int)(character - L'a') + 10;
    }
    if (character >= L'A' && character <= L'F') {
        return (int)(character - L'A') + 10;
    }
    return -1;
}

static int parse_hex_part(
    const wchar_t *text,
    size_t length,
    ULONGLONG *value
)
{
    size_t index;
    ULONGLONG parsed = 0ULL;

    if (text == NULL || value == NULL || length == 0U || length > 16U) {
        return 0;
    }

    for (index = 0U; index < length; ++index) {
        int digit = parse_hex_digit(text[index]);
        if (digit < 0) {
            return 0;
        }
        parsed = (parsed << 4U) | (ULONGLONG)digit;
    }

    *value = parsed;
    return 1;
}

static int parse_guid_filter(const wchar_t *text, GUID *guid)
{
    size_t length;
    size_t start = 0U;
    ULONGLONG value;
    size_t index;

    if (text == NULL || guid == NULL) {
        return 0;
    }

    length = wcslen(text);
    if (length == 38U && text[0] == L'{' && text[37] == L'}') {
        start = 1U;
    } else if (length != 36U) {
        return 0;
    }

    if (text[start + 8U] != L'-' ||
        text[start + 13U] != L'-' ||
        text[start + 18U] != L'-' ||
        text[start + 23U] != L'-') {
        return 0;
    }

    if (!parse_hex_part(text + start, 8U, &value)) {
        return 0;
    }
    guid->Data1 = (ULONG)value;

    if (!parse_hex_part(text + start + 9U, 4U, &value)) {
        return 0;
    }
    guid->Data2 = (USHORT)value;

    if (!parse_hex_part(text + start + 14U, 4U, &value)) {
        return 0;
    }
    guid->Data3 = (USHORT)value;

    if (!parse_hex_part(text + start + 19U, 2U, &value)) {
        return 0;
    }
    guid->Data4[0] = (BYTE)value;

    if (!parse_hex_part(text + start + 21U, 2U, &value)) {
        return 0;
    }
    guid->Data4[1] = (BYTE)value;

    for (index = 0U; index < 6U; ++index) {
        if (!parse_hex_part(text + start + 24U + (index * 2U), 2U, &value)) {
            return 0;
        }
        guid->Data4[index + 2U] = (BYTE)value;
    }

    return 1;
}

static int run_etl_search(int argc, wchar_t **argv)
{
    COMTRACE_ETL_FILTERS filters;
    int index;

    ZeroMemory(&filters, sizeof(filters));
    filters.sample_limit = 100U;

    for (index = 3; index < argc; ++index) {
        const wchar_t *option = argv[index];
        ULONG value;

        if (wcscmp(option, L"--com") == 0) {
            filters.com_only = TRUE;
        } else if (wcscmp(option, L"--ole") == 0) {
            filters.ole_only = TRUE;
        } else if (wcscmp(option, L"--provider") == 0 ||
                   wcscmp(option, L"--event-id") == 0 ||
                   wcscmp(option, L"--pid") == 0 ||
                   wcscmp(option, L"--tid") == 0 ||
                   wcscmp(option, L"--max-events") == 0) {
            const wchar_t *argument;

            if (index + 1 >= argc) {
                fwprintf(stderr, L"Missing value for %ls.\n", option);
                return EXIT_FAILURE;
            }

            argument = argv[++index];

            if (wcscmp(option, L"--provider") == 0) {
                if (!parse_guid_filter(argument, &filters.provider_id)) {
                    fwprintf(stderr, L"Invalid provider GUID.\n");
                    return EXIT_FAILURE;
                }
                filters.has_provider_id = TRUE;
            } else if (wcscmp(option, L"--event-id") == 0) {
                if (!parse_decimal_value(argument, 65535UL, &value)) {
                    fwprintf(stderr, L"Invalid event ID.\n");
                    return EXIT_FAILURE;
                }
                filters.event_id = (USHORT)value;
                filters.has_event_id = TRUE;
            } else if (wcscmp(option, L"--pid") == 0) {
                if (!parse_decimal_value(argument, 0xFFFFFFFFUL, &filters.process_id)) {
                    fwprintf(stderr, L"Invalid process ID.\n");
                    return EXIT_FAILURE;
                }
                filters.has_process_id = TRUE;
            } else if (wcscmp(option, L"--tid") == 0) {
                if (!parse_decimal_value(argument, 0xFFFFFFFFUL, &filters.thread_id)) {
                    fwprintf(stderr, L"Invalid thread ID.\n");
                    return EXIT_FAILURE;
                }
                filters.has_thread_id = TRUE;
            } else {
                if (!parse_decimal_value(argument, 10000UL, &filters.sample_limit) ||
                    filters.sample_limit == 0U) {
                    fwprintf(stderr, L"Invalid max-events value. Use 1 to 10000.\n");
                    return EXIT_FAILURE;
                }
            }
        } else {
            fwprintf(stderr, L"Unknown search option: %ls\n", option);
            return EXIT_FAILURE;
        }
    }

    if (filters.com_only && filters.ole_only) {
        fwprintf(stderr, L"Use either --com or --ole, not both.\n");
        return EXIT_FAILURE;
    }

    return comtrace_search_etl(argv[2], &filters);
}

static const wchar_t *trace_compare_status_name(
    COMTRACE_TRACE_COMPARE_STATUS status
)
{
    switch (status) {
    case COMTRACE_TRACE_COMPARE_SUCCESS:
        return L"success";
    case COMTRACE_TRACE_COMPARE_INVALID_ARGUMENT:
        return L"invalid argument";
    case COMTRACE_TRACE_COMPARE_UNSUPPORTED_FORMAT:
        return L"unsupported file format";
    case COMTRACE_TRACE_COMPARE_ETL_OPEN_FAILED:
        return L"ETL open failed";
    case COMTRACE_TRACE_COMPARE_ETL_PROCESS_FAILED:
        return L"ETL processing failed";
    case COMTRACE_TRACE_COMPARE_ARCHIVE_FAILED:
        return L"CTRA read failed";
    case COMTRACE_TRACE_COMPARE_CATALOG_FAILED:
        return L"event catalog update failed";
    case COMTRACE_TRACE_COMPARE_ALLOCATION_FAILED:
        return L"memory allocation failed";
    case COMTRACE_TRACE_COMPARE_COUNT_OVERFLOW:
        return L"counter overflow";
    default:
        return L"unknown comparison error";
    }
}

static int run_trace_comparison(
    const wchar_t *left_path,
    const wchar_t *right_path,
    int json_output
)
{
    COMTRACE_TRACE_COMPARISON comparison;
    COMTRACE_TRACE_COMPARE_STATUS status;

    ZeroMemory(&comparison, sizeof(comparison));

    status = comtrace_trace_compare(left_path, right_path, &comparison);
    if (status != COMTRACE_TRACE_COMPARE_SUCCESS) {
        fwprintf(
            stderr,
            L"Trace comparison failed: %ls.\n",
            trace_compare_status_name(status)
        );
        return EXIT_FAILURE;
    }

    if (json_output) {
        comtrace_trace_comparison_write_json(
            &comparison,
            left_path,
            right_path
        );
    } else {
        comtrace_trace_comparison_write_text(&comparison);
    }

    comtrace_trace_comparison_destroy(&comparison);
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

    if (argc >= 3 && wcscmp(argv[1], L"--search") == 0) {
        return run_etl_search(argc, argv);
    }

    if ((argc == 4 || argc == 5) && wcscmp(argv[1], L"--compare") == 0) {
        int json_output = argc == 5 && wcscmp(argv[4], L"--json") == 0;

        if (argc == 5 && !json_output) {
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }

        return run_trace_comparison(argv[2], argv[3], json_output);
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