#include <windows.h>
#include <objidl.h>
#include <ole2.h>
#include <shlobj_core.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    IDataObject data_object_iface;
    LONG references;
} COMTRACE_TEST_DATA_OBJECT;

typedef struct {
    IDropTarget drop_target_iface;
    LONG references;
} COMTRACE_TEST_DROP_TARGET;

static const IID COMTRACE_IID_IUNKNOWN = {
    0x00000000, 0x0000, 0x0000,
    { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 }
};

static const IID COMTRACE_IID_IDATAOBJECT = {
    0x0000010e, 0x0000, 0x0000,
    { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 }
};

static const IID COMTRACE_IID_IDROPTARGET = {
    0x00000122, 0x0000, 0x0000,
    { 0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 }
};

static HRESULT STDMETHODCALLTYPE data_object_query_interface(
    IDataObject *data_object_iface,
    REFIID interface_id,
    void **result
);

static HRESULT STDMETHODCALLTYPE drop_target_query_interface(
    IDropTarget *drop_target_iface,
    REFIID interface_id,
    void **result
);

static ULONG STDMETHODCALLTYPE drop_target_add_ref(IDropTarget *drop_target_iface);
static ULONG STDMETHODCALLTYPE drop_target_release(IDropTarget *drop_target_iface);

static HRESULT STDMETHODCALLTYPE drop_target_drag_enter(
    IDropTarget *drop_target_iface,
    IDataObject *data_object,
    DWORD key_state,
    POINTL point,
    DWORD *effect
);

static HRESULT STDMETHODCALLTYPE drop_target_drag_over(
    IDropTarget *drop_target_iface,
    DWORD key_state,
    POINTL point,
    DWORD *effect
);

static HRESULT STDMETHODCALLTYPE drop_target_drag_leave(IDropTarget *drop_target_iface);

static HRESULT STDMETHODCALLTYPE drop_target_drop(
    IDropTarget *drop_target_iface,
    IDataObject *data_object,
    DWORD key_state,
    POINTL point,
    DWORD *effect
);

static ULONG STDMETHODCALLTYPE data_object_add_ref(IDataObject *data_object_iface);
static ULONG STDMETHODCALLTYPE data_object_release(IDataObject *data_object_iface);

static HRESULT STDMETHODCALLTYPE data_object_get_data(
    IDataObject *data_object_iface,
    FORMATETC *format,
    STGMEDIUM *medium
);

static HRESULT STDMETHODCALLTYPE data_object_get_data_here(
    IDataObject *data_object_iface,
    FORMATETC *format,
    STGMEDIUM *medium
);

static HRESULT STDMETHODCALLTYPE data_object_query_get_data(
    IDataObject *data_object_iface,
    FORMATETC *format
);

static HRESULT STDMETHODCALLTYPE data_object_get_canonical_format_etc(
    IDataObject *data_object_iface,
    FORMATETC *input,
    FORMATETC *output
);

static HRESULT STDMETHODCALLTYPE data_object_set_data(
    IDataObject *data_object_iface,
    FORMATETC *format,
    STGMEDIUM *medium,
    BOOL release
);

static HRESULT STDMETHODCALLTYPE data_object_enum_format_etc(
    IDataObject *data_object_iface,
    DWORD direction,
    IEnumFORMATETC **enumerator
);

static HRESULT STDMETHODCALLTYPE data_object_d_advise(
    IDataObject *data_object_iface,
    FORMATETC *format,
    DWORD flags,
    IAdviseSink *sink,
    DWORD *connection
);

static HRESULT STDMETHODCALLTYPE data_object_d_unadvise(
    IDataObject *data_object_iface,
    DWORD connection
);

static HRESULT STDMETHODCALLTYPE data_object_enum_d_advise(
    IDataObject *data_object_iface,
    IEnumSTATDATA **enumerator
);

static IDataObjectVtbl data_object_vtable = {
    data_object_query_interface,
    data_object_add_ref,
    data_object_release,
    data_object_get_data,
    data_object_get_data_here,
    data_object_query_get_data,
    data_object_get_canonical_format_etc,
    data_object_set_data,
    data_object_enum_format_etc,
    data_object_d_advise,
    data_object_d_unadvise,
    data_object_enum_d_advise
};

static IDropTargetVtbl drop_target_vtable = {
    drop_target_query_interface,
    drop_target_add_ref,
    drop_target_release,
    drop_target_drag_enter,
    drop_target_drag_over,
    drop_target_drag_leave,
    drop_target_drop
};

static HRESULT STDMETHODCALLTYPE data_object_query_interface(
    IDataObject *data_object_iface,
    REFIID interface_id,
    void **result
)
{
    if (result == NULL || interface_id == NULL) {
        return E_POINTER;
    }

    *result = NULL;

    if (IsEqualGUID(interface_id, &COMTRACE_IID_IUNKNOWN) ||
        IsEqualGUID(interface_id, &COMTRACE_IID_IDATAOBJECT)) {
        *result = data_object_iface;
        data_object_add_ref(data_object_iface);
        return S_OK;
    }

    return E_NOINTERFACE;
}

static HRESULT STDMETHODCALLTYPE drop_target_query_interface(
    IDropTarget *drop_target_iface,
    REFIID interface_id,
    void **result
)
{
    if (result == NULL || interface_id == NULL) {
        return E_POINTER;
    }

    *result = NULL;
    if (IsEqualGUID(interface_id, &COMTRACE_IID_IUNKNOWN) ||
        IsEqualGUID(interface_id, &COMTRACE_IID_IDROPTARGET)) {
        *result = drop_target_iface;
        drop_target_add_ref(drop_target_iface);
        return S_OK;
    }

    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE drop_target_add_ref(IDropTarget *drop_target_iface)
{
    COMTRACE_TEST_DROP_TARGET *target = (COMTRACE_TEST_DROP_TARGET *)drop_target_iface;
    return (ULONG)InterlockedIncrement(&target->references);
}

static ULONG STDMETHODCALLTYPE drop_target_release(IDropTarget *drop_target_iface)
{
    COMTRACE_TEST_DROP_TARGET *target = (COMTRACE_TEST_DROP_TARGET *)drop_target_iface;
    LONG references = InterlockedDecrement(&target->references);
    if (references == 0) {
        HeapFree(GetProcessHeap(), 0, target);
    }
    return (ULONG)references;
}

static HRESULT STDMETHODCALLTYPE drop_target_drag_enter(
    IDropTarget *drop_target_iface,
    IDataObject *data_object,
    DWORD key_state,
    POINTL point,
    DWORD *effect
)
{
    UNREFERENCED_PARAMETER(drop_target_iface);
    UNREFERENCED_PARAMETER(data_object);
    UNREFERENCED_PARAMETER(key_state);
    UNREFERENCED_PARAMETER(point);
    if (effect == NULL) {
        return E_POINTER;
    }
    *effect = DROPEFFECT_COPY;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE drop_target_drag_over(
    IDropTarget *drop_target_iface,
    DWORD key_state,
    POINTL point,
    DWORD *effect
)
{
    UNREFERENCED_PARAMETER(drop_target_iface);
    UNREFERENCED_PARAMETER(key_state);
    UNREFERENCED_PARAMETER(point);
    if (effect == NULL) {
        return E_POINTER;
    }
    *effect = DROPEFFECT_COPY;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE drop_target_drag_leave(IDropTarget *drop_target_iface)
{
    UNREFERENCED_PARAMETER(drop_target_iface);
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE drop_target_drop(
    IDropTarget *drop_target_iface,
    IDataObject *data_object,
    DWORD key_state,
    POINTL point,
    DWORD *effect
)
{
    UNREFERENCED_PARAMETER(drop_target_iface);
    UNREFERENCED_PARAMETER(data_object);
    UNREFERENCED_PARAMETER(key_state);
    UNREFERENCED_PARAMETER(point);
    if (effect == NULL) {
        return E_POINTER;
    }
    *effect = DROPEFFECT_COPY;
    return S_OK;
}

static IDropTarget *create_drop_target(void)
{
    COMTRACE_TEST_DROP_TARGET *target = (COMTRACE_TEST_DROP_TARGET *)HeapAlloc(
        GetProcessHeap(),
        HEAP_ZERO_MEMORY,
        sizeof(*target)
    );

    if (target == NULL) {
        return NULL;
    }

    target->drop_target_iface.lpVtbl = &drop_target_vtable;
    target->references = 1;
    return &target->drop_target_iface;
}

static ULONG STDMETHODCALLTYPE data_object_add_ref(IDataObject *data_object_iface)
{
    COMTRACE_TEST_DATA_OBJECT *object =
        (COMTRACE_TEST_DATA_OBJECT *)data_object_iface;

    return (ULONG)InterlockedIncrement(&object->references);
}

static ULONG STDMETHODCALLTYPE data_object_release(IDataObject *data_object_iface)
{
    COMTRACE_TEST_DATA_OBJECT *object =
        (COMTRACE_TEST_DATA_OBJECT *)data_object_iface;
    LONG references = InterlockedDecrement(&object->references);

    if (references == 0) {
        HeapFree(GetProcessHeap(), 0, object);
    }

    return (ULONG)references;
}

static HRESULT STDMETHODCALLTYPE data_object_query_get_data(
    IDataObject *data_object_iface,
    FORMATETC *format
)
{
    UNREFERENCED_PARAMETER(data_object_iface);

    if (format == NULL) {
        return E_POINTER;
    }

    if (format->cfFormat != CF_UNICODETEXT) {
        return DV_E_FORMATETC;
    }

    if (format->dwAspect != DVASPECT_CONTENT) {
        return DV_E_DVASPECT;
    }

    if (format->lindex != -1) {
        return DV_E_LINDEX;
    }

    if ((format->tymed & TYMED_HGLOBAL) == 0) {
        return DV_E_TYMED;
    }

    return S_OK;
}

static HRESULT STDMETHODCALLTYPE data_object_get_data(
    IDataObject *data_object_iface,
    FORMATETC *format,
    STGMEDIUM *medium
)
{
    static const wchar_t clipboard_text[] = L"ComTrace OLE validation";
    HGLOBAL global;
    void *buffer;
    HRESULT result;

    result = data_object_query_get_data(data_object_iface, format);
    if (FAILED(result)) {
        return result;
    }

    if (medium == NULL) {
        return E_POINTER;
    }

    ZeroMemory(medium, sizeof(*medium));
    global = GlobalAlloc(GMEM_MOVEABLE, sizeof(clipboard_text));
    if (global == NULL) {
        return E_OUTOFMEMORY;
    }

    buffer = GlobalLock(global);
    if (buffer == NULL) {
        GlobalFree(global);
        return E_OUTOFMEMORY;
    }

    CopyMemory(buffer, clipboard_text, sizeof(clipboard_text));
    GlobalUnlock(global);

    medium->tymed = TYMED_HGLOBAL;
    medium->hGlobal = global;
    medium->pUnkForRelease = NULL;
    return S_OK;
}

static HRESULT STDMETHODCALLTYPE data_object_get_data_here(
    IDataObject *data_object_iface,
    FORMATETC *format,
    STGMEDIUM *medium
)
{
    UNREFERENCED_PARAMETER(data_object_iface);
    UNREFERENCED_PARAMETER(format);
    UNREFERENCED_PARAMETER(medium);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE data_object_get_canonical_format_etc(
    IDataObject *data_object_iface,
    FORMATETC *input,
    FORMATETC *output
)
{
    UNREFERENCED_PARAMETER(data_object_iface);

    if (input == NULL || output == NULL) {
        return E_POINTER;
    }

    *output = *input;
    output->ptd = NULL;
    return DATA_S_SAMEFORMATETC;
}

static HRESULT STDMETHODCALLTYPE data_object_set_data(
    IDataObject *data_object_iface,
    FORMATETC *format,
    STGMEDIUM *medium,
    BOOL release
)
{
    UNREFERENCED_PARAMETER(data_object_iface);
    UNREFERENCED_PARAMETER(format);
    UNREFERENCED_PARAMETER(medium);
    UNREFERENCED_PARAMETER(release);
    return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE data_object_enum_format_etc(
    IDataObject *data_object_iface,
    DWORD direction,
    IEnumFORMATETC **enumerator
)
{
    FORMATETC format;

    UNREFERENCED_PARAMETER(data_object_iface);

    if (enumerator == NULL) {
        return E_POINTER;
    }

    *enumerator = NULL;
    if (direction != DATADIR_GET) {
        return E_NOTIMPL;
    }

    ZeroMemory(&format, sizeof(format));
    format.cfFormat = CF_UNICODETEXT;
    format.dwAspect = DVASPECT_CONTENT;
    format.lindex = -1;
    format.tymed = TYMED_HGLOBAL;

    return SHCreateStdEnumFmtEtc(1U, &format, enumerator);
}

static HRESULT STDMETHODCALLTYPE data_object_d_advise(
    IDataObject *data_object_iface,
    FORMATETC *format,
    DWORD flags,
    IAdviseSink *sink,
    DWORD *connection
)
{
    UNREFERENCED_PARAMETER(data_object_iface);
    UNREFERENCED_PARAMETER(format);
    UNREFERENCED_PARAMETER(flags);
    UNREFERENCED_PARAMETER(sink);

    if (connection != NULL) {
        *connection = 0U;
    }

    return OLE_E_ADVISENOTSUPPORTED;
}

static HRESULT STDMETHODCALLTYPE data_object_d_unadvise(
    IDataObject *data_object_iface,
    DWORD connection
)
{
    UNREFERENCED_PARAMETER(data_object_iface);
    UNREFERENCED_PARAMETER(connection);
    return OLE_E_ADVISENOTSUPPORTED;
}

static HRESULT STDMETHODCALLTYPE data_object_enum_d_advise(
    IDataObject *data_object_iface,
    IEnumSTATDATA **enumerator
)
{
    UNREFERENCED_PARAMETER(data_object_iface);

    if (enumerator != NULL) {
        *enumerator = NULL;
    }

    return OLE_E_ADVISENOTSUPPORTED;
}

static IDataObject *create_data_object(void)
{
    COMTRACE_TEST_DATA_OBJECT *object =
        (COMTRACE_TEST_DATA_OBJECT *)HeapAlloc(
            GetProcessHeap(),
            HEAP_ZERO_MEMORY,
            sizeof(*object)
        );

    if (object == NULL) {
        return NULL;
    }

    object->data_object_iface.lpVtbl = &data_object_vtable;
    object->references = 1;
    return &object->data_object_iface;
}

static int run_clipboard_activity(IDataObject *data_object)
{
    IDataObject *previous = NULL;
    IDataObject *current = NULL;
    HRESULT result;
    HRESULT restore_result;
    int operation_succeeded = 0;
    int restore_succeeded = 0;

    result = OleGetClipboard(&previous);
    if (FAILED(result)) {
        printf("OleGetClipboard before test failed: 0x%08lx\n", (unsigned long)result);
        return 0;
    }

    result = OleSetClipboard(data_object);
    if (FAILED(result)) {
        printf("OleSetClipboard failed: 0x%08lx\n", (unsigned long)result);
        goto cleanup;
    }

    result = OleIsCurrentClipboard(data_object);
    if (result != S_OK) {
        printf("OleIsCurrentClipboard failed: 0x%08lx\n", (unsigned long)result);
        goto restore;
    }

    result = OleGetClipboard(&current);
    if (FAILED(result) || current == NULL) {
        printf("OleGetClipboard during test failed: 0x%08lx\n", (unsigned long)result);
        goto restore;
    }

    current->lpVtbl->Release(current);
    current = NULL;
    operation_succeeded = 1;

restore:
    {
        unsigned int attempt;

        for (attempt = 0; attempt < 5U; ++attempt) {
            restore_result = OleSetClipboard(previous);
            if (SUCCEEDED(restore_result)) {
                restore_succeeded = 1;
                break;
            }

            if (previous != NULL &&
                OleIsCurrentClipboard(previous) == S_OK) {
                restore_succeeded = 1;
                break;
            }

            Sleep(50U);
        }
    }

    if (!restore_succeeded) {
        printf("Restoring the previous clipboard failed: 0x%08lx\n",
            (unsigned long)restore_result);
        goto cleanup;
    }

    if (previous != NULL) {
        unsigned int attempt;
        int flush_succeeded = 0;

        for (attempt = 0; attempt < 5U; ++attempt) {
            restore_result = OleFlushClipboard();
            if (SUCCEEDED(restore_result)) {
                flush_succeeded = 1;
                break;
            }

            Sleep(50U);
        }

        if (!flush_succeeded) {
            printf("Flushing the restored clipboard failed: 0x%08lx\n",
                (unsigned long)restore_result);
            goto cleanup;
        }
    }

cleanup:
    if (current != NULL) {
        current->lpVtbl->Release(current);
    }
    if (previous != NULL) {
        previous->lpVtbl->Release(previous);
    }

    return operation_succeeded && restore_succeeded;
}

int main(int argc, char **argv)
{
    IDataObject *data_object = NULL;
    IDropTarget *drop_target = NULL;
    HWND drop_window = NULL;
    FORMATETC format;
    STGMEDIUM medium;
    HRESULT result;
    BOOL drop_registered = FALSE;
    BOOL interactive_mode = FALSE;
    int exit_code = 1;

    if (argc == 2 && strcmp(argv[1], "--interactive") == 0) {
        interactive_mode = TRUE;
    } else if (argc != 1) {
        puts("Usage: ComTraceOleActivityFixture [--interactive]");
        return 1;
    }

    result = OleInitialize(NULL);
    if (FAILED(result)) {
        printf("OleInitialize failed: 0x%08lx\n", (unsigned long)result);
        return 1;
    }

    puts("OLE fixture: initialized.");
    fflush(stdout);

    data_object = create_data_object();
    if (data_object == NULL) {
        puts("Unable to allocate the OLE data object.");
        goto cleanup;
    }

    ZeroMemory(&format, sizeof(format));
    format.cfFormat = CF_UNICODETEXT;
    format.dwAspect = DVASPECT_CONTENT;
    format.lindex = -1;
    format.tymed = TYMED_HGLOBAL;

    ZeroMemory(&medium, sizeof(medium));
    puts("OLE fixture: reading IDataObject data.");
    fflush(stdout);
    result = data_object->lpVtbl->GetData(data_object, &format, &medium);
    if (FAILED(result)) {
        printf("IDataObject::GetData failed: 0x%08lx\n", (unsigned long)result);
        goto cleanup;
    }

    ReleaseStgMedium(&medium);

    puts("OLE fixture: IDataObject read succeeded.");
    fflush(stdout);

    if (interactive_mode) {
        puts("OLE fixture: exercising clipboard APIs.");
        fflush(stdout);
        if (!run_clipboard_activity(data_object)) {
            goto cleanup;
        }
        puts("OLE clipboard API activity succeeded.");
    }

    drop_target = create_drop_target();
    if (drop_target == NULL) {
        puts("Unable to allocate the OLE drop target.");
        goto cleanup;
    }

    drop_window = CreateWindowExW(
        0U,
        L"STATIC",
        L"ComTrace OLE validation",
        WS_POPUP,
        0,
        0,
        1,
        1,
        NULL,
        NULL,
        GetModuleHandleW(NULL),
        NULL
    );
    if (drop_window == NULL) {
        printf("CreateWindowExW failed: %lu\n", (unsigned long)GetLastError());
        goto cleanup;
    }

    puts("OLE fixture: registering drop target.");
    fflush(stdout);
    result = RegisterDragDrop(drop_window, drop_target);
    if (FAILED(result)) {
        printf("RegisterDragDrop failed: 0x%08lx\n", (unsigned long)result);
        goto cleanup;
    }
    drop_registered = TRUE;
    puts("OLE fixture: drop target registered.");
    fflush(stdout);

    result = RevokeDragDrop(drop_window);
    if (FAILED(result)) {
        printf("RevokeDragDrop failed: 0x%08lx\n", (unsigned long)result);
        goto cleanup;
    }
    drop_registered = FALSE;
    puts("OLE fixture: drop target revoked.");
    fflush(stdout);

    exit_code = 0;

cleanup:
    if (drop_registered && drop_window != NULL) {
        RevokeDragDrop(drop_window);
    }
    if (drop_window != NULL) {
        DestroyWindow(drop_window);
    }
    if (drop_target != NULL) {
        drop_target->lpVtbl->Release(drop_target);
    }
    if (data_object != NULL) {
        data_object->lpVtbl->Release(data_object);
    }

    OleUninitialize();

    if (exit_code == 0) {
        puts("OLE IDataObject activity succeeded.");
        puts("OLE drop target registration succeeded.");
    }

    return exit_code;
}
