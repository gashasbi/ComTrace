#include "comtrace_dde.h"

#include <TraceLoggingProvider.h>

TRACELOGGING_DEFINE_PROVIDER(
    g_comtrace_dde_provider,
    "ComTrace.DDE",
    (0x7b6d4a21, 0x9e35, 0x4c80, 0xa1, 0x72, 0x53, 0x94, 0xb6, 0xd8, 0x0f, 0x21)
);

static volatile LONG64 g_next_conversation_id = 0;
static volatile LONG64 g_next_transaction_id = 0;

ULONG comtrace_dde_initialize(void)
{
    return TraceLoggingRegister(g_comtrace_dde_provider);
}

void comtrace_dde_shutdown(void)
{
    TraceLoggingUnregister(g_comtrace_dde_provider);
}

ULONGLONG comtrace_dde_conversation_opened(
    ULONG dde_instance_id,
    BOOL is_client,
    ULONGLONG correlation_id,
    const wchar_t *service,
    const wchar_t *topic
)
{
    LONG64 conversation_id;

    conversation_id = InterlockedIncrement64(&g_next_conversation_id);
    if (conversation_id <= 0) {
        return 0ULL;
    }

    TraceLoggingWrite(
        g_comtrace_dde_provider,
        "DdeConversationOpened",
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingKeyword(0x1ULL),
        TraceLoggingUInt64((ULONGLONG)conversation_id, "ConversationId"),
        TraceLoggingUInt32(dde_instance_id, "DdeInstanceId"),
        TraceLoggingUInt64(correlation_id, "CorrelationId"),
        TraceLoggingString(is_client ? "client" : "server", "Role"),
        TraceLoggingWideString(service != NULL ? service : L"", "Service"),
        TraceLoggingWideString(topic != NULL ? topic : L"", "Topic")
    );

    return (ULONGLONG)conversation_id;
}

void comtrace_dde_conversation_closed(
    ULONGLONG conversation_id,
    ULONG result
)
{
    if (conversation_id == 0ULL) {
        return;
    }

    TraceLoggingWrite(
        g_comtrace_dde_provider,
        "DdeConversationClosed",
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingKeyword(0x1ULL),
        TraceLoggingUInt64(conversation_id, "ConversationId"),
        TraceLoggingUInt32(result, "Result")
    );
}

ULONGLONG comtrace_dde_transaction_started(
    ULONGLONG conversation_id,
    ULONGLONG conversation_correlation_id,
    ULONGLONG transaction_correlation_id,
    BOOL is_client,
    ULONG dde_transaction_id,
    ULONG transaction_type,
    ULONG format,
    BOOL asynchronous,
    const wchar_t *item
)
{
    LONG64 transaction_instance_id;

    if (conversation_id == 0ULL) {
        return 0ULL;
    }

    transaction_instance_id = InterlockedIncrement64(&g_next_transaction_id);
    if (transaction_instance_id <= 0) {
        return 0ULL;
    }

    TraceLoggingWrite(
        g_comtrace_dde_provider,
        "DdeTransactionStarted",
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingKeyword(0x1ULL),
        TraceLoggingUInt64(conversation_id, "ConversationId"),
        TraceLoggingUInt64(conversation_correlation_id, "ConversationCorrelationId"),
        TraceLoggingUInt64((ULONGLONG)transaction_instance_id, "TransactionInstanceId"),
        TraceLoggingUInt64(transaction_correlation_id, "TransactionCorrelationId"),
        TraceLoggingBool(is_client, "IsClient"),
        TraceLoggingUInt32(dde_transaction_id, "DdeTransactionId"),
        TraceLoggingUInt32(transaction_type, "TransactionType"),
        TraceLoggingUInt32(format, "Format"),
        TraceLoggingBool(asynchronous, "Asynchronous"),
        TraceLoggingWideString(item != NULL ? item : L"", "Item")
    );

    return (ULONGLONG)transaction_instance_id;
}

void comtrace_dde_transaction_completed(
    ULONGLONG conversation_id,
    ULONGLONG conversation_correlation_id,
    ULONGLONG transaction_instance_id,
    ULONGLONG transaction_correlation_id,
    BOOL is_client,
    ULONG dde_transaction_id,
    ULONG result,
    ULONG data_size,
    BOOL succeeded
)
{
    if (conversation_id == 0ULL || transaction_instance_id == 0ULL) {
        return;
    }

    TraceLoggingWrite(
        g_comtrace_dde_provider,
        "DdeTransactionCompleted",
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingKeyword(0x1ULL),
        TraceLoggingUInt64(conversation_id, "ConversationId"),
        TraceLoggingUInt64(conversation_correlation_id, "ConversationCorrelationId"),
        TraceLoggingUInt64(transaction_instance_id, "TransactionInstanceId"),
        TraceLoggingUInt64(transaction_correlation_id, "TransactionCorrelationId"),
        TraceLoggingBool(is_client, "IsClient"),
        TraceLoggingUInt32(dde_transaction_id, "DdeTransactionId"),
        TraceLoggingUInt32(result, "Result"),
        TraceLoggingUInt32(data_size, "DataSize"),
        TraceLoggingBool(succeeded, "Succeeded")
    );
}

void comtrace_dde_callback_received(
    ULONGLONG conversation_id,
    ULONGLONG conversation_correlation_id,
    ULONGLONG transaction_correlation_id,
    BOOL is_server_callback,
    ULONG transaction_type,
    ULONG format,
    const wchar_t *item,
    ULONG data_size
)
{
    if (conversation_id == 0ULL) {
        return;
    }

    TraceLoggingWrite(
        g_comtrace_dde_provider,
        "DdeCallbackReceived",
        TraceLoggingLevel(WINEVENT_LEVEL_INFO),
        TraceLoggingKeyword(0x1ULL),
        TraceLoggingUInt64(conversation_id, "ConversationId"),
        TraceLoggingUInt64(conversation_correlation_id, "ConversationCorrelationId"),
        TraceLoggingUInt64(transaction_correlation_id, "TransactionCorrelationId"),
        TraceLoggingBool(is_server_callback, "IsServerCallback"),
        TraceLoggingUInt32(transaction_type, "TransactionType"),
        TraceLoggingUInt32(format, "Format"),
        TraceLoggingWideString(item != NULL ? item : L"", "Item"),
        TraceLoggingUInt32(data_size, "DataSize")
    );
}
