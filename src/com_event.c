#include <string.h>
#include "com_event.h"

static const GUID com_perf_provider = {
    0xB8D6861BUL,
    0xD20FU,
    0x4EECU,
    {0xBB, 0xAE, 0x87, 0xE0, 0xDD, 0x80, 0x60, 0x2B}
};

typedef struct {
    USHORT event_id;
    UCHAR version;
    UCHAR opcode;
    const wchar_t *operation;
    const wchar_t *phase;
} COM_EVENT_MAPPING;

static const COM_EVENT_MAPPING mappings[] = {
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

int comtrace_classify_com_event(
    const GUID *provider_id,
    USHORT event_id,
    UCHAR version,
    UCHAR opcode,
    const wchar_t **operation,
    const wchar_t **phase
)
{
    size_t index;

    if (operation != NULL) {
        *operation = NULL;
    }
    if (phase != NULL) {
        *phase = NULL;
    }

    if (provider_id == NULL ||
        operation == NULL ||
        phase == NULL ||
        memcmp(provider_id, &com_perf_provider, sizeof(GUID)) != 0) {
        return 0;
    }

    for (index = 0U; index < sizeof(mappings) / sizeof(mappings[0]); ++index) {
        if (mappings[index].event_id == event_id &&
            mappings[index].version == version &&
            mappings[index].opcode == opcode) {
            *operation = mappings[index].operation;
            *phase = mappings[index].phase;
            return 1;
        }
    }

    return 0;
}

static const GUID com_rundown_provider = {
    0x2957313DUL,
    0xFCAAU,
    0x5D4AU,
    {0x2F, 0x69, 0x32, 0xCE, 0x5F, 0x0A, 0xC4, 0x4E}
};

static const wchar_t *const com_rundown_event_names[] = {
    L"RundownInstrumentation",
    L"RundownInstrumentation2",
    L"RundownInstrumentation3",
    L"RundownInstrumentation4",
    L"RundownInstrumentation5",
    L"RundownInstrumentation6",
    L"RundownInstrumentation7_V1",
    L"RundownInstrumentation8",
    L"RundownInstrumentation9",
    L"RundownInstrumentation10_V1",
    L"RundownInstrumentation11_V1",
    L"RundownInstrumentation12",
    L"RundownInstrumentation13",
    L"RundownInstrumentation14",
    L"RundownInstrumentation15",
    L"RundownInstrumentation16",
    L"RundownInstrumentation17",
    L"RundownInstrumentation18",
    L"RundownInstrumentation19",
    L"RundownInstrumentation20",
    L"RundownInstrumentation21",
    L"RundownInstrumentation22",
    L"RundownInstrumentation23",
    L"RundownInstrumentation24",
    L"RundownInstrumentation25"
};

int comtrace_classify_com_rundown_event(
    const GUID *provider_id,
    USHORT event_id,
    UCHAR version,
    UCHAR opcode,
    const wchar_t **event_name
)
{
    UCHAR expected_version;

    if (event_name != NULL) {
        *event_name = NULL;
    }

    if (provider_id == NULL ||
        event_name == NULL ||
        memcmp(provider_id, &com_rundown_provider, sizeof(GUID)) != 0 ||
        event_id < 1U ||
        event_id > sizeof(com_rundown_event_names) / sizeof(com_rundown_event_names[0]) ||
        opcode != 0U) {
        return 0;
    }

    expected_version =
        (event_id == 7U || event_id == 10U || event_id == 11U) ? 1U : 0U;

    if (version != expected_version) {
        return 0;
    }

    *event_name = com_rundown_event_names[event_id - 1U];
    return 1;
}
