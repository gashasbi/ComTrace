#include <stdio.h>
#include <wchar.h>
#include "com_event.h"

typedef struct {
    USHORT event_id;
    UCHAR version;
    UCHAR opcode;
    const wchar_t *operation;
    const wchar_t *phase;
} TestCase;

static const GUID com_perf_provider = {
    0xB8D6861BUL,
    0xD20FU,
    0x4EECU,
    {0xBB, 0xAE, 0x87, 0xE0, 0xDD, 0x80, 0x60, 0x2B}
};

static const GUID other_provider = {
    0xD4263C98UL,
    0x310CU,
    0x4D97U,
    {0xBA, 0x39, 0xB5, 0x53, 0x54, 0xF0, 0x85, 0x84}
};

static int run_mapping_case(const TestCase *test_case)
{
    const wchar_t *operation = NULL;
    const wchar_t *phase = NULL;

    if (!comtrace_classify_com_event(
            &com_perf_provider,
            test_case->event_id,
            test_case->version,
            test_case->opcode,
            &operation,
            &phase)) {
        fwprintf(stderr, L"Expected classification for event %u\n",
            (unsigned int)test_case->event_id);
        return 0;
    }

    if (wcscmp(operation, test_case->operation) != 0 ||
        wcscmp(phase, test_case->phase) != 0) {
        fwprintf(stderr, L"Unexpected classification for event %u\n",
            (unsigned int)test_case->event_id);
        return 0;
    }

    return 1;
}

static int run_rejection_case(
    const GUID *provider_id,
    USHORT event_id,
    UCHAR version,
    UCHAR opcode
)
{
    const wchar_t *operation = NULL;
    const wchar_t *phase = NULL;

    return !comtrace_classify_com_event(
        provider_id,
        event_id,
        version,
        opcode,
        &operation,
        &phase
    );
}

int main(void)
{
    static const TestCase cases[] = {
        {1U, 0U, 1U, L"Windows_Runtime_ActivateInstance_InProc", L"start"},
        {2U, 0U, 2U, L"Windows_Runtime_ActivateInstance_InProc", L"stop"},
        {5U, 0U, 1U, L"COM_CreateInstance", L"start"},
        {6U, 0U, 2U, L"COM_CreateInstance", L"stop"},
        {7U, 0U, 1U, L"Windows_Runtime_GetActivationFactory_InProc", L"start"},
        {8U, 0U, 2U, L"Windows_Runtime_GetActivationFactory_InProc", L"stop"},
        {9U, 0U, 1U, L"Windows_Runtime_GetActivationFactory_OutOfProc", L"start"},
        {10U, 0U, 2U, L"Windows_Runtime_GetActivationFactory_OutOfProc", L"stop"},
        {11U, 0U, 1U, L"COM_GetClassObject", L"start"},
        {12U, 0U, 2U, L"COM_GetClassObject", L"stop"},
        {25U, 0U, 1U, L"COM_CreateInstance_App", L"start"},
        {26U, 0U, 2U, L"COM_CreateInstance_App", L"stop"},
        {31U, 0U, 1U, L"Windows_Runtime_GetActivationFactory_InprocServerImpl", L"start"},
        {32U, 0U, 2U, L"Windows_Runtime_GetActivationFactory_InprocServerImpl", L"stop"},
        {33U, 0U, 1U, L"Windows_Runtime_ActivateInstance_InprocServerImpl", L"start"},
        {34U, 0U, 2U, L"Windows_Runtime_ActivateInstance_InprocServerImpl", L"stop"},
        {35U, 0U, 1U, L"COM_GetClassFactory_InprocServerImpl", L"start"},
        {36U, 0U, 2U, L"COM_GetClassFactory_InprocServerImpl", L"stop"},
        {37U, 0U, 1U, L"COM_CreateInstance_InprocServerImpl", L"start"},
        {38U, 0U, 2U, L"COM_CreateInstance_InprocServerImpl", L"stop"},
        {39U, 1U, 1U, L"COM_ClientSyncCall", L"start"},
        {40U, 0U, 2U, L"COM_ClientSyncCall", L"stop"},
        {41U, 1U, 1U, L"COM_ClientAsyncCall", L"start"},
        {42U, 0U, 2U, L"COM_ClientAsyncCall", L"stop"},
        {43U, 2U, 1U, L"COM_ServerSyncCall", L"start"},
        {44U, 0U, 2U, L"COM_ServerSyncCall", L"stop"},
        {45U, 1U, 1U, L"COM_ServerAsyncCall", L"start"},
        {46U, 0U, 2U, L"COM_ServerAsyncCall", L"stop"},
        {47U, 0U, 0U, L"COM_FreeUnusedLibrary", L"info"},
        {48U, 0U, 0U, L"COM_ApartmentUninitialize", L"info"},
        {49U, 0U, 0U, L"COM_ApartmentInitialize", L"info"}
    };
    size_t index;

    for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        if (!run_mapping_case(&cases[index])) {
            return 1;
        }
    }

    if (!run_rejection_case(&other_provider, 5U, 0U, 1U) ||
        !run_rejection_case(&com_perf_provider, 5U, 1U, 1U) ||
        !run_rejection_case(&com_perf_provider, 5U, 0U, 2U) ||
        !run_rejection_case(&com_perf_provider, 11U, 1U, 1U) ||
        !run_rejection_case(&com_perf_provider, 12U, 0U, 1U) ||
        !run_rejection_case(&com_perf_provider, 25U, 1U, 1U) ||
        !run_rejection_case(&com_perf_provider, 26U, 0U, 1U) ||
        !run_rejection_case(&com_perf_provider, 37U, 0U, 2U) ||
        !run_rejection_case(&com_perf_provider, 38U, 0U, 1U) ||
        !run_rejection_case(&com_perf_provider, 39U, 0U, 1U) ||
        !run_rejection_case(&com_perf_provider, 41U, 0U, 1U) ||
        !run_rejection_case(&com_perf_provider, 42U, 0U, 1U) ||
        !run_rejection_case(&com_perf_provider, 43U, 1U, 1U) ||
        !run_rejection_case(&com_perf_provider, 45U, 0U, 1U) ||
        !run_rejection_case(&com_perf_provider, 46U, 0U, 1U) ||
        !run_rejection_case(&com_perf_provider, 47U, 0U, 1U) ||
        !run_rejection_case(&com_perf_provider, 48U, 0U, 2U) ||
        !run_rejection_case(&com_perf_provider, 999U, 0U, 1U) ||
        !run_rejection_case(&com_perf_provider, 5U, 0U, 0U)) {
        fputs("Unexpected classification for an unsupported event\n", stderr);
        return 1;
    }

    printf("COM event classification tests passed: %zu mappings and 19 rejection cases\n",
        sizeof(cases) / sizeof(cases[0]));
    return 0;
}