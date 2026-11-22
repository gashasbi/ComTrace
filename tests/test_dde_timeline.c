#include "dde_timeline.h"

#include <stdio.h>

static int test_conversation_and_transaction_lifecycle(void)
{
    COMTRACE_DDE_TIMELINE timeline;
    COMTRACE_DDE_TIMELINE_EVENT event;
    const COMTRACE_DDE_TIMELINE_RECORD *conversation;
    const COMTRACE_DDE_TIMELINE_RECORD *transaction;

    comtrace_dde_timeline_init(&timeline);
    ZeroMemory(&event, sizeof(event));
    event.process_id = 100U;
    event.thread_id = 200U;
    event.timestamp = 100ULL;
    event.conversation_id = 10ULL;
    event.conversation_correlation_id = 500ULL;
    event.role = COMTRACE_DDE_ROLE_CLIENT;
    event.type = COMTRACE_DDE_TIMELINE_CONVERSATION_OPENED;
    event.service = L"Service";
    event.topic = L"Topic";
    if (comtrace_dde_timeline_add_event(&timeline, &event) !=
        COMTRACE_DDE_TIMELINE_SUCCESS) {
        comtrace_dde_timeline_destroy(&timeline);
        return 0;
    }

    event.type = COMTRACE_DDE_TIMELINE_TRANSACTION_STARTED;
    event.timestamp = 200ULL;
    event.transaction_instance_id = 20ULL;
    event.transaction_correlation_id = 600ULL;
    event.transaction_type = 0x00B0U;
    event.format = 1U;
    event.item = L"Document";
    if (comtrace_dde_timeline_add_event(&timeline, &event) !=
        COMTRACE_DDE_TIMELINE_SUCCESS) {
        comtrace_dde_timeline_destroy(&timeline);
        return 0;
    }

    event.type = COMTRACE_DDE_TIMELINE_TRANSACTION_COMPLETED;
    event.timestamp = 350ULL;
    event.result = 0U;
    event.data_size = 128U;
    event.succeeded = TRUE;
    if (comtrace_dde_timeline_add_event(&timeline, &event) !=
        COMTRACE_DDE_TIMELINE_SUCCESS) {
        comtrace_dde_timeline_destroy(&timeline);
        return 0;
    }

    event.type = COMTRACE_DDE_TIMELINE_CONVERSATION_CLOSED;
    event.timestamp = 400ULL;
    event.result = 0U;
    if (comtrace_dde_timeline_add_event(&timeline, &event) !=
        COMTRACE_DDE_TIMELINE_SUCCESS) {
        comtrace_dde_timeline_destroy(&timeline);
        return 0;
    }

    conversation = comtrace_dde_timeline_get(&timeline, 0U);
    transaction = comtrace_dde_timeline_get(&timeline, 1U);
    if (conversation == NULL || transaction == NULL ||
        !conversation->has_opened || !conversation->has_closed ||
        conversation->opened_timestamp != 100ULL ||
        conversation->closed_timestamp != 400ULL ||
        conversation->conversation_correlation_id != 500ULL ||
        transaction->has_start == FALSE ||
        transaction->has_completion == FALSE ||
        transaction->has_duration == FALSE ||
        transaction->duration_ticks != 150ULL ||
        transaction->data_size != 128U ||
        !transaction->succeeded ||
        transaction->transaction_type != 0x00B0U ||
        transaction->format != 1U ||
        wcscmp(conversation->service, L"Service") != 0 ||
        wcscmp(conversation->topic, L"Topic") != 0) {
        comtrace_dde_timeline_destroy(&timeline);
        return 0;
    }

    comtrace_dde_timeline_destroy(&timeline);
    return 1;
}

static int test_explicit_cross_process_exchange(void)
{
    COMTRACE_DDE_TIMELINE timeline;
    COMTRACE_DDE_TIMELINE_EVENT event;
    const COMTRACE_DDE_EXCHANGE *exchange;

    comtrace_dde_timeline_init(&timeline);
    ZeroMemory(&event, sizeof(event));
    event.type = COMTRACE_DDE_TIMELINE_TRANSACTION_STARTED;
    event.process_id = 100U;
    event.thread_id = 200U;
    event.timestamp = 1000ULL;
    event.conversation_id = 10ULL;
    event.conversation_correlation_id = 500ULL;
    event.transaction_instance_id = 20ULL;
    event.transaction_correlation_id = 600ULL;
    event.transaction_type = 0x00B0U;
    event.format = 1U;
    event.role = COMTRACE_DDE_ROLE_CLIENT;
    event.item = L"Document";
    if (comtrace_dde_timeline_add_event(&timeline, &event) !=
        COMTRACE_DDE_TIMELINE_SUCCESS) {
        comtrace_dde_timeline_destroy(&timeline);
        return 0;
    }

    event.type = COMTRACE_DDE_TIMELINE_CALLBACK_RECEIVED;
    event.process_id = 300U;
    event.thread_id = 400U;
    event.timestamp = 1200ULL;
    event.conversation_id = 11ULL;
    event.transaction_instance_id = 0ULL;
    event.role = COMTRACE_DDE_ROLE_SERVER;
    event.data_size = 128U;
    if (comtrace_dde_timeline_add_event(&timeline, &event) !=
            COMTRACE_DDE_TIMELINE_SUCCESS ||
        comtrace_dde_timeline_correlate(&timeline) !=
            COMTRACE_DDE_TIMELINE_SUCCESS) {
        comtrace_dde_timeline_destroy(&timeline);
        return 0;
    }

    exchange = comtrace_dde_timeline_get_exchange(&timeline, 0U);
    if (exchange == NULL ||
        exchange->client_record_index != 0U ||
        exchange->server_record_index != 1U ||
        exchange->transaction_correlation_id != 600ULL ||
        comtrace_dde_timeline_exchange_count(&timeline) != 1U) {
        comtrace_dde_timeline_destroy(&timeline);
        return 0;
    }

    comtrace_dde_timeline_destroy(&timeline);
    return 1;
}

static int test_ambiguous_and_unmatched_events(void)
{
    COMTRACE_DDE_TIMELINE timeline;
    COMTRACE_DDE_TIMELINE_EVENT event;

    comtrace_dde_timeline_init(&timeline);
    ZeroMemory(&event, sizeof(event));
    event.type = COMTRACE_DDE_TIMELINE_CALLBACK_RECEIVED;
    event.process_id = 100U;
    event.thread_id = 1U;
    event.timestamp = 10ULL;
    event.conversation_id = 1ULL;
    event.transaction_correlation_id = 77ULL;
    event.transaction_type = 0x00B0U;
    event.format = 1U;
    event.role = COMTRACE_DDE_ROLE_SERVER;
    event.item = L"Item";
    if (comtrace_dde_timeline_add_event(&timeline, &event) !=
        COMTRACE_DDE_TIMELINE_SUCCESS) {
        comtrace_dde_timeline_destroy(&timeline);
        return 0;
    }

    event.process_id = 101U;
    event.conversation_id = 2ULL;
    event.timestamp = 11ULL;
    if (comtrace_dde_timeline_add_event(&timeline, &event) !=
        COMTRACE_DDE_TIMELINE_SUCCESS) {
        comtrace_dde_timeline_destroy(&timeline);
        return 0;
    }

    event.type = COMTRACE_DDE_TIMELINE_TRANSACTION_STARTED;
    event.process_id = 200U;
    event.conversation_id = 3ULL;
    event.transaction_instance_id = 4ULL;
    event.role = COMTRACE_DDE_ROLE_CLIENT;
    event.timestamp = 12ULL;
    if (comtrace_dde_timeline_add_event(&timeline, &event) !=
            COMTRACE_DDE_TIMELINE_SUCCESS ||
        comtrace_dde_timeline_correlate(&timeline) !=
            COMTRACE_DDE_TIMELINE_SUCCESS ||
        comtrace_dde_timeline_exchange_count(&timeline) != 0U ||
        timeline.ambiguous_correlations != 3ULL) {
        comtrace_dde_timeline_destroy(&timeline);
        return 0;
    }

    event.type = COMTRACE_DDE_TIMELINE_TRANSACTION_COMPLETED;
    event.process_id = 300U;
    event.conversation_id = 5ULL;
    event.transaction_instance_id = 6ULL;
    event.timestamp = 20ULL;
    if (comtrace_dde_timeline_add_event(&timeline, &event) !=
            COMTRACE_DDE_TIMELINE_SUCCESS ||
        timeline.unmatched_completions != 1ULL) {
        comtrace_dde_timeline_destroy(&timeline);
        return 0;
    }

    comtrace_dde_timeline_destroy(&timeline);
    return 1;
}

int main(void)
{
    if (!test_conversation_and_transaction_lifecycle() ||
        !test_explicit_cross_process_exchange() ||
        !test_ambiguous_and_unmatched_events()) {
        fputs("DDE timeline tests failed.\n", stderr);
        return 1;
    }

    puts("DDE timeline tests passed.");
    return 0;
}
