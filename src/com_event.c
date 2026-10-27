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
    UCHAR opcode;
    const wchar_t *operation;
    const wchar_t *phase;
} COM_EVENT_MAPPING;

static const COM_EVENT_MAPPING mappings[] = {
    {1U, 1U, L"Windows_Runtime_ActivateInstance_InProc", L"start"},
    {2U, 2U, L"Windows_Runtime_ActivateInstance_InProc", L"stop"},
    {5U, 1U, L"COM_CreateInstance", L"start"},
    {6U, 2U, L"COM_CreateInstance", L"stop"},
    {7U, 1U, L"Windows_Runtime_GetActivationFactory_InProc", L"start"},
    {8U, 2U, L"Windows_Runtime_GetActivationFactory_InProc", L"stop"},
    {9U, 1U, L"Windows_Runtime_GetActivationFactory_OutOfProc", L"start"},
    {10U, 2U, L"Windows_Runtime_GetActivationFactory_OutOfProc", L"stop"},
    {31U, 1U, L"Windows_Runtime_GetActivationFactory_InprocServerImpl", L"start"},
    {32U, 2U, L"Windows_Runtime_GetActivationFactory_InprocServerImpl", L"stop"},
    {33U, 1U, L"Windows_Runtime_ActivateInstance_InprocServerImpl", L"start"},
    {34U, 2U, L"Windows_Runtime_ActivateInstance_InprocServerImpl", L"stop"},
    {35U, 1U, L"COM_GetClassFactory_InprocServerImpl", L"start"},
    {36U, 2U, L"COM_GetClassFactory_InprocServerImpl", L"stop"}
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

    if (provider_id == NULL ||
        operation == NULL ||
        phase == NULL ||
        memcmp(provider_id, &com_perf_provider, sizeof(GUID)) != 0 ||
        version != 0U) {
        return 0;
    }

    for (index = 0U; index < sizeof(mappings) / sizeof(mappings[0]); ++index) {
        if (mappings[index].event_id == event_id &&
            mappings[index].opcode == opcode) {
            *operation = mappings[index].operation;
            *phase = mappings[index].phase;
            return 1;
        }
    }

    return 0;
}
