#ifndef COMTRACE_DDE_H
#define COMTRACE_DDE_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

ULONG comtrace_dde_initialize(void);
void comtrace_dde_shutdown(void);

ULONGLONG comtrace_dde_conversation_opened(
    ULONG dde_instance_id,
    BOOL is_client,
    ULONGLONG correlation_id,
    const wchar_t *service,
    const wchar_t *topic
);

void comtrace_dde_conversation_closed(
    ULONGLONG conversation_id,
    ULONG result
);

ULONGLONG comtrace_dde_transaction_started(
    ULONGLONG conversation_id,
    ULONGLONG transaction_correlation_id,
    ULONG dde_transaction_id,
    ULONG transaction_type,
    ULONG format,
    BOOL asynchronous,
    const wchar_t *item
);

void comtrace_dde_transaction_completed(
    ULONGLONG conversation_id,
    ULONGLONG transaction_instance_id,
    ULONGLONG transaction_correlation_id,
    ULONG dde_transaction_id,
    ULONG result,
    ULONG data_size,
    BOOL succeeded
);

void comtrace_dde_callback_received(
    ULONGLONG conversation_id,
    ULONGLONG transaction_correlation_id,
    ULONG transaction_type,
    ULONG format,
    const wchar_t *item,
    ULONG data_size
);

#ifdef __cplusplus
}
#endif

#endif
