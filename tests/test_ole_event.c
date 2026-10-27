#include "ole_event.h"

#include <stdio.h>
#include <wchar.h>

static const GUID OLE_PERF_PROVIDER_ID = {
    0x84958368,
    0x7DA7,
    0x49A0,
    {0xB3, 0x3D, 0x07, 0xFA, 0xBB, 0x87, 0x96, 0x26}
};

static const GUID OTHER_PROVIDER_ID = {
    0x11111111,
    0x2222,
    0x3333,
    {0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB}
};

static const wchar_t *const EXPECTED_OPERATIONS[] = {
    L"OLE_Clipboard_GetClipboard",
    L"OLE_Clipboard_SetClipboard",
    L"OLE_Clipboard_Flush",
    L"OLE_Clipboard_IsCurrentClipboard",
    L"OLE_DragDrop_DoDragDrop",
    L"OLE_DragDrop_Register",
    L"OLE_DragDrop_Unregister",
    L"OLE_DragDrop_RegisterInternal",
    L"OLE_DragDrop_UnregisterInternal",
    L"OLE_DragDrop_CreateDragOperationInternal",
    L"OLE_DragDrop_DragOperation_UpdatePosition",
    L"OLE_DragDrop_DragOperation_SetDataObject",
    L"OLE_DragDrop_DragOperation_DoDrop",
    L"OLE_DragDrop_DragOperation_Cancel"
};

static int expect_rejected(
    const GUID *provider_id,
    USHORT event_id,
    UCHAR version,
    UCHAR opcode,
    const wchar_t **operation,
    const wchar_t **phase)
{
    return !comtrace_classify_ole_event(
        provider_id, event_id, version, opcode, operation, phase);
}

int main(void)
{
    const wchar_t *operation = NULL;
    const wchar_t *phase = NULL;
    unsigned int event_id;
    unsigned int failures = 0;

    for (event_id = 1; event_id <= 28; ++event_id) {
        UCHAR opcode = (event_id % 2 == 1) ? 1 : 2;
        const wchar_t *expected_phase =
            (event_id % 2 == 1) ? L"Start" : L"Stop";
        const wchar_t *expected_operation =
            EXPECTED_OPERATIONS[(event_id - 1) / 2];

        operation = NULL;
        phase = NULL;

        if (!comtrace_classify_ole_event(
                &OLE_PERF_PROVIDER_ID,
                (USHORT)event_id,
                0,
                opcode,
                &operation,
                &phase)) {
            ++failures;
            continue;
        }

        if (wcscmp(operation, expected_operation) != 0 ||
            wcscmp(phase, expected_phase) != 0) {
            ++failures;
        }
    }

    if (!expect_rejected(&OTHER_PROVIDER_ID, 1, 0, 1, &operation, &phase)) {
        ++failures;
    }
    if (!expect_rejected(&OLE_PERF_PROVIDER_ID, 1, 1, 1, &operation, &phase)) {
        ++failures;
    }
    if (!expect_rejected(&OLE_PERF_PROVIDER_ID, 1, 0, 2, &operation, &phase)) {
        ++failures;
    }
    if (!expect_rejected(&OLE_PERF_PROVIDER_ID, 0, 0, 1, &operation, &phase)) {
        ++failures;
    }
    if (comtrace_classify_ole_event(
            NULL, 1, 0, 1, &operation, &phase)) {
        ++failures;
    }
    if (comtrace_classify_ole_event(
            &OLE_PERF_PROVIDER_ID, 1, 0, 1, NULL, &phase)) {
        ++failures;
    }
    if (comtrace_classify_ole_event(
            &OLE_PERF_PROVIDER_ID, 1, 0, 1, &operation, NULL)) {
        ++failures;
    }

    if (failures != 0) {
        fwprintf(stderr, L"OLE event tests failed: %u\n", failures);
        return 1;
    }

    wprintf(L"OLE event tests passed: 28 mappings and rejection cases\n");
    return 0;
}
