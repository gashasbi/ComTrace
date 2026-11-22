#include <windows.h>
#include <evntrace.h>
#include <winmeta.h>
#include <TraceLoggingProvider.h>
#include "comtrace_lifetime.h"

#define COMTRACE_LIFETIME_KEYWORD 0x1ULL

TRACELOGGING_DEFINE_PROVIDER(
    g_comtrace_lifetime_provider,
    "ComTrace.COMLifetime",
    (0x6f3b5e21, 0x7c14, 0x4a90, 0x9d, 0x42, 0x81, 0x31, 0x6c, 0xe2, 0xa7, 0x50)
);

static volatile LONG64 g_next_object_instance_id = 0;

ULONG comtrace_lifetime_initialize(void)
{
    return TraceLoggingRegister(g_comtrace_lifetime_provider);
}

void comtrace_lifetime_shutdown(void)
{
    TraceLoggingUnregister(g_comtrace_lifetime_provider);
}

ULONGLONG comtrace_lifetime_object_created(
    const GUID *class_id,
    const GUID *interface_id,
    ULONG initial_reference_count
)
{
    LONGLONG next_id;

    if (class_id == NULL || interface_id == NULL) {
        return 0ULL;
    }

    next_id = InterlockedIncrement64(&g_next_object_instance_id);
    if (next_id <= 0) {
        return 0ULL;
    }

    TraceLoggingWrite(
        g_comtrace_lifetime_provider,
        "ComObjectCreated",
        TraceLoggingLevel(TRACE_LEVEL_INFORMATION),
        TraceLoggingKeyword(COMTRACE_LIFETIME_KEYWORD),
        TraceLoggingUInt64((ULONGLONG)next_id, "ObjectInstanceId"),
        TraceLoggingGuid(*class_id, "ClassId"),
        TraceLoggingGuid(*interface_id, "InterfaceId"),
        TraceLoggingUInt32(initial_reference_count, "InitialReferenceCount")
    );

    return (ULONGLONG)next_id;
}

void comtrace_lifetime_add_ref(
    ULONGLONG object_instance_id,
    const GUID *interface_id,
    ULONG reference_count_result
)
{
    if (object_instance_id == 0ULL || interface_id == NULL) {
        return;
    }

    TraceLoggingWrite(
        g_comtrace_lifetime_provider,
        "ComAddRef",
        TraceLoggingLevel(TRACE_LEVEL_INFORMATION),
        TraceLoggingKeyword(COMTRACE_LIFETIME_KEYWORD),
        TraceLoggingUInt64(object_instance_id, "ObjectInstanceId"),
        TraceLoggingGuid(*interface_id, "InterfaceId"),
        TraceLoggingUInt32(reference_count_result, "ReferenceCountResult")
    );
}

void comtrace_lifetime_release(
    ULONGLONG object_instance_id,
    const GUID *interface_id,
    ULONG reference_count_result
)
{
    if (object_instance_id == 0ULL || interface_id == NULL) {
        return;
    }

    TraceLoggingWrite(
        g_comtrace_lifetime_provider,
        "ComRelease",
        TraceLoggingLevel(TRACE_LEVEL_INFORMATION),
        TraceLoggingKeyword(COMTRACE_LIFETIME_KEYWORD),
        TraceLoggingUInt64(object_instance_id, "ObjectInstanceId"),
        TraceLoggingGuid(*interface_id, "InterfaceId"),
        TraceLoggingUInt32(reference_count_result, "ReferenceCountResult")
    );
}

void comtrace_lifetime_bind_native_identity(
    ULONGLONG object_instance_id,
    ULONGLONG native_object_id,
    const GUID *interface_id,
    const GUID *native_ipid
)
{
    GUID empty_ipid = {0};

    if (object_instance_id == 0ULL ||
        native_object_id == 0ULL ||
        interface_id == NULL) {
        return;
    }

    TraceLoggingWrite(
        g_comtrace_lifetime_provider,
        "ComObjectNativeIdentityBound",
        TraceLoggingLevel(TRACE_LEVEL_INFORMATION),
        TraceLoggingKeyword(COMTRACE_LIFETIME_KEYWORD),
        TraceLoggingUInt64(object_instance_id, "ObjectInstanceId"),
        TraceLoggingUInt64(native_object_id, "NativeObjectId"),
        TraceLoggingGuid(*interface_id, "InterfaceId"),
        TraceLoggingGuid(native_ipid == NULL ? empty_ipid : *native_ipid, "Ipid")
    );
}

void comtrace_lifetime_object_destroyed(
    ULONGLONG object_instance_id,
    const GUID *interface_id
)
{
    if (object_instance_id == 0ULL || interface_id == NULL) {
        return;
    }

    TraceLoggingWrite(
        g_comtrace_lifetime_provider,
        "ComObjectDestroyed",
        TraceLoggingLevel(TRACE_LEVEL_INFORMATION),
        TraceLoggingKeyword(COMTRACE_LIFETIME_KEYWORD),
        TraceLoggingUInt64(object_instance_id, "ObjectInstanceId"),
        TraceLoggingGuid(*interface_id, "InterfaceId")
    );
}
