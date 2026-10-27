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

    if (operation == NULL || phase == NULL ||
        wcscmp(operation, test_case->operation) != 0 ||
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
    const wchar_t *operation = L"unchanged_operation";
    const wchar_t *phase = L"unchanged_phase";

    if (comtrace_classify_com_event(
            provider_id,
            event_id,
            version,
            opcode,
            &operation,
            &phase)) {
        fwprintf(stderr, L"Unexpected classification for event %u\n",
            (unsigned int)event_id);
        return 0;
    }

    if (wcscmp(operation, L"unchanged_operation") != 0) {
        fputs("Operation output changed after rejected classification\n", stderr);
        return 0;
    }

    if (wcscmp(phase, L"unchanged_phase") != 0) {
        fputs("Phase output changed after rejected classification\n", stderr);
        return 0;
    }

    return 1;
}

static int run_null_argument_cases(void)
{
    const wchar_t *operation;
    const wchar_t *phase;

    operation = L"unchanged_operation";
    phase = L"unchanged_phase";
    if (comtrace_classify_com_event(
            NULL, 5U, 0U, 1U, &operation, &phase) ||
        wcscmp(operation, L"unchanged_operation") != 0 ||
        wcscmp(phase, L"unchanged_phase") != 0) {
        fputs("Null provider handling failed\n", stderr);
        return 0;
    }

    operation = L"unchanged_operation";
    phase = L"unchanged_phase";
    if (comtrace_classify_com_event(
            &com_perf_provider, 5U, 0U, 1U, NULL, &phase) ||
        wcscmp(phase, L"unchanged_phase") != 0) {
        fputs("Null operation output handling failed\n", stderr);
        return 0;
    }

    operation = L"unchanged_operation";
    phase = L"unchanged_phase";
    if (comtrace_classify_com_event(
            &com_perf_provider, 5U, 0U, 1U, &operation, NULL) ||
        wcscmp(operation, L"unchanged_operation") != 0) {
        fputs("Null phase output handling failed\n", stderr);
        return 0;
    }

    if (comtrace_classify_com_event(
            NULL, 5U, 0U, 1U, NULL, NULL)) {
        fputs("Multiple null arguments were unexpectedly accepted\n", stderr);
        return 0;
    }

    return 1;
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
    size_t rejection_count = 0U;

    for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        const TestCase *test_case = &cases[index];
        UCHAR wrong_version = (UCHAR)(test_case->version + 1U);
        UCHAR wrong_opcode = (UCHAR)(test_case->opcode == 1U ? 2U : 1U);

        if (!run_mapping_case(test_case) ||
            !run_rejection_case(
                &com_perf_provider,
                test_case->event_id,
                wrong_version,
                test_case->opcode) ||
            !run_rejection_case(
                &com_perf_provider,
                test_case->event_id,
                test_case->version,
                wrong_opcode)) {
            return 1;
        }

        rejection_count += 2U;
    }

    if (!run_rejection_case(&other_provider, 5U, 0U, 1U) ||
        !run_rejection_case(&com_perf_provider, 0U, 0U, 1U) ||
        !run_rejection_case(&com_perf_provider, 3U, 0U, 1U) ||
        !run_rejection_case(&com_perf_provider, 4U, 0U, 1U) ||
        !run_rejection_case(&com_perf_provider, 13U, 0U, 1U) ||
        !run_rejection_case(&com_perf_provider, 24U, 0U, 2U) ||
        !run_rejection_case(&com_perf_provider, 27U, 0U, 1U) ||
        !run_rejection_case(&com_perf_provider, 28U, 0U, 2U) ||
        !run_rejection_case(&com_perf_provider, 50U, 0U, 1U) ||
        !run_rejection_case(&com_perf_provider, 55U, 0U, 2U) ||
        !run_rejection_case(&com_perf_provider, 999U, 0U, 1U) ||
        !run_null_argument_cases()) {
        fputs("Unexpected classification for an unsupported event or null argument\n", stderr);
        return 1;
    }

    rejection_count += 11U;

    printf("COM event classification tests passed: %zu mappings and %zu rejection cases\n",
        sizeof(cases) / sizeof(cases[0]), rejection_count);
    return 0;
}
