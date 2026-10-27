#include "ole_event.h"

static const GUID OLE_PERF_PROVIDER_ID = {
    0x84958368,
    0x7DA7,
    0x49A0,
    {0xB3, 0x3D, 0x07, 0xFA, 0xBB, 0x87, 0x96, 0x26}
};

static const wchar_t *const OLE_PERF_OPERATIONS[] = {
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

int comtrace_classify_ole_event(
    const GUID *provider_id,
    USHORT event_id,
    UCHAR version,
    UCHAR opcode,
    const wchar_t **operation,
    const wchar_t **phase)
{
    if (provider_id == NULL || operation == NULL || phase == NULL) {
        return 0;
    }

    if (!IsEqualGUID(provider_id, &OLE_PERF_PROVIDER_ID) || version != 0) {
        return 0;
    }

    if (event_id < 1 || event_id > 28) {
        return 0;
    }

    if ((event_id % 2 == 1 && opcode != 1) ||
        (event_id % 2 == 0 && opcode != 2)) {
        return 0;
    }

    *operation = OLE_PERF_OPERATIONS[(event_id - 1) / 2];
    *phase = (event_id % 2 == 1) ? L"Start" : L"Stop";
    return 1;
}
