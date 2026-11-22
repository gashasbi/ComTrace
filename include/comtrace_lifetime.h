#ifndef COMTRACE_LIFETIME_H
#define COMTRACE_LIFETIME_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

ULONG comtrace_lifetime_initialize(void);
void comtrace_lifetime_shutdown(void);

ULONGLONG comtrace_lifetime_object_created(
    const GUID *class_id,
    const GUID *interface_id,
    ULONG initial_reference_count
);

void comtrace_lifetime_add_ref(
    ULONGLONG object_instance_id,
    const GUID *interface_id,
    ULONG reference_count
);

void comtrace_lifetime_release(
    ULONGLONG object_instance_id,
    const GUID *interface_id,
    ULONG reference_count
);

void comtrace_lifetime_object_destroyed(
    ULONGLONG object_instance_id,
    const GUID *interface_id
);

void comtrace_lifetime_bind_native_identity(
    ULONGLONG object_instance_id,
    ULONGLONG native_object_id,
    const GUID *interface_id,
    const GUID *native_ipid
);

#ifdef __cplusplus
}
#endif

#endif
