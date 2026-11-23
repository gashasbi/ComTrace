#include "dde_timeline.h"

#include <stdlib.h>
#include <wchar.h>

static void increment_count(ULONGLONG *value)
{
    if (*value != (ULONGLONG)-1) {
        ++(*value);
    }
}

static int ensure_record_capacity(COMTRACE_DDE_TIMELINE *timeline)
{
    size_t capacity;
    COMTRACE_DDE_TIMELINE_RECORD *records;

    if (timeline->record_count < timeline->record_capacity) {
        return 1;
    }

    capacity = timeline->record_capacity == 0U ? 16U : timeline->record_capacity * 2U;
    if (capacity < timeline->record_capacity ||
        capacity > (size_t)-1 / sizeof(*records)) {
        return 0;
    }

    records = (COMTRACE_DDE_TIMELINE_RECORD *)realloc(
        timeline->records,
        capacity * sizeof(*records)
    );
    if (records == NULL) {
        return 0;
    }

    timeline->records = records;
    timeline->record_capacity = capacity;
    return 1;
}

static int ensure_exchange_capacity(COMTRACE_DDE_TIMELINE *timeline)
{
    size_t capacity;
    COMTRACE_DDE_EXCHANGE *exchanges;

    if (timeline->exchange_count < timeline->exchange_capacity) {
        return 1;
    }

    capacity = timeline->exchange_capacity == 0U ? 8U : timeline->exchange_capacity * 2U;
    if (capacity < timeline->exchange_capacity ||
        capacity > (size_t)-1 / sizeof(*exchanges)) {
        return 0;
    }

    exchanges = (COMTRACE_DDE_EXCHANGE *)realloc(
        timeline->exchanges,
        capacity * sizeof(*exchanges)
    );
    if (exchanges == NULL) {
        return 0;
    }

    timeline->exchanges = exchanges;
    timeline->exchange_capacity = capacity;
    return 1;
}

static int copy_text(wchar_t *destination, size_t capacity, const wchar_t *source)
{
    size_t length;

    if (source == NULL) {
        destination[0] = L'\0';
        return 1;
    }

    length = wcslen(source);
    if (length >= capacity) {
        return 0;
    }

    CopyMemory(destination, source, (length + 1U) * sizeof(wchar_t));
    return 1;
}

static int same_conversation(
    const COMTRACE_DDE_TIMELINE_RECORD *record,
    const COMTRACE_DDE_TIMELINE_EVENT *event
)
{
    return record->process_id == event->process_id &&
        record->conversation_id == event->conversation_id;
}

static int same_transaction(
    const COMTRACE_DDE_TIMELINE_RECORD *record,
    const COMTRACE_DDE_TIMELINE_EVENT *event
)
{
    return same_conversation(record, event) &&
        record->role == event->role &&
        record->transaction_instance_id != 0ULL &&
        record->transaction_instance_id == event->transaction_instance_id;
}

static int transaction_metadata_mismatch(
    const COMTRACE_DDE_TIMELINE_RECORD *record,
    const COMTRACE_DDE_TIMELINE_EVENT *event
)
{
    if ((record->conversation_correlation_id != 0ULL &&
         event->conversation_correlation_id != 0ULL &&
         record->conversation_correlation_id != event->conversation_correlation_id) ||
        (record->transaction_correlation_id != 0ULL &&
         event->transaction_correlation_id != 0ULL &&
         record->transaction_correlation_id != event->transaction_correlation_id) ||
        (record->dde_transaction_id != 0U && event->dde_transaction_id != 0U &&
         record->dde_transaction_id != event->dde_transaction_id) ||
        (record->transaction_type != 0U && event->transaction_type != 0U &&
         record->transaction_type != event->transaction_type) ||
        (record->format != 0U && event->format != 0U &&
         record->format != event->format) ||
        (record->item[0] != L'\0' && event->item != NULL &&
         event->item[0] != L'\0' && _wcsicmp(record->item, event->item) != 0)) {
        return 1;
    }
    return 0;
}

static void copy_transaction_fields(
    COMTRACE_DDE_TIMELINE_RECORD *record,
    const COMTRACE_DDE_TIMELINE_EVENT *event
)
{
    if (record->conversation_correlation_id == 0ULL) {
        record->conversation_correlation_id = event->conversation_correlation_id;
    }
    if (record->transaction_correlation_id == 0ULL) {
        record->transaction_correlation_id = event->transaction_correlation_id;
    }
    if (record->dde_transaction_id == 0U) {
        record->dde_transaction_id = event->dde_transaction_id;
    }
    if (record->transaction_type == 0U) {
        record->transaction_type = event->transaction_type;
    }
    if (record->format == 0U) {
        record->format = event->format;
    }
    if (record->item[0] == L'\0' && event->item != NULL) {
        (void)copy_text(record->item, _countof(record->item), event->item);
    }
}

static int records_can_correlate(
    const COMTRACE_DDE_TIMELINE_RECORD *left,
    const COMTRACE_DDE_TIMELINE_RECORD *right
)
{
    if (left->transaction_correlation_id == 0ULL ||
        left->transaction_correlation_id != right->transaction_correlation_id ||
        left->role == right->role ||
        (left->process_id == right->process_id &&
         left->conversation_id == right->conversation_id) ||
        (left->transaction_type != 0U && right->transaction_type != 0U &&
         left->transaction_type != right->transaction_type) ||
        (left->format != 0U && right->format != 0U &&
         left->format != right->format) ||
        (left->item[0] != L'\0' && right->item[0] != L'\0' &&
         _wcsicmp(left->item, right->item) != 0)) {
        return 0;
    }

    if ((left->conversation_correlation_id == 0ULL) !=
        (right->conversation_correlation_id == 0ULL)) {
        return 0;
    }

    if (left->conversation_correlation_id != 0ULL &&
        left->conversation_correlation_id != right->conversation_correlation_id) {
        return 0;
    }

    return 1;
}

void comtrace_dde_timeline_init(COMTRACE_DDE_TIMELINE *timeline)
{
    if (timeline != NULL) {
        ZeroMemory(timeline, sizeof(*timeline));
    }
}

void comtrace_dde_timeline_destroy(COMTRACE_DDE_TIMELINE *timeline)
{
    if (timeline == NULL) {
        return;
    }

    free(timeline->records);
    free(timeline->exchanges);
    ZeroMemory(timeline, sizeof(*timeline));
}

COMTRACE_DDE_TIMELINE_STATUS comtrace_dde_timeline_add_event(
    COMTRACE_DDE_TIMELINE *timeline,
    const COMTRACE_DDE_TIMELINE_EVENT *event
)
{
    size_t index;
    COMTRACE_DDE_TIMELINE_RECORD *record;

    if (timeline == NULL || event == NULL || event->conversation_id == 0ULL) {
        return COMTRACE_DDE_TIMELINE_INVALID_ARGUMENT;
    }

    if (event->type < COMTRACE_DDE_TIMELINE_CONVERSATION_OPENED ||
        event->type > COMTRACE_DDE_TIMELINE_CALLBACK_RECEIVED) {
        return COMTRACE_DDE_TIMELINE_INVALID_ARGUMENT;
    }

    if ((event->type == COMTRACE_DDE_TIMELINE_CONVERSATION_OPENED ||
         event->type == COMTRACE_DDE_TIMELINE_TRANSACTION_STARTED ||
         event->type == COMTRACE_DDE_TIMELINE_TRANSACTION_COMPLETED ||
         event->type == COMTRACE_DDE_TIMELINE_CALLBACK_RECEIVED) &&
        event->role != COMTRACE_DDE_ROLE_CLIENT &&
        event->role != COMTRACE_DDE_ROLE_SERVER) {
        return COMTRACE_DDE_TIMELINE_INVALID_ARGUMENT;
    }

    if ((event->type == COMTRACE_DDE_TIMELINE_TRANSACTION_STARTED ||
         event->type == COMTRACE_DDE_TIMELINE_TRANSACTION_COMPLETED) &&
        event->transaction_instance_id == 0ULL) {
        return COMTRACE_DDE_TIMELINE_INVALID_ARGUMENT;
    }

    if (event->type == COMTRACE_DDE_TIMELINE_CONVERSATION_CLOSED) {
        size_t match_index = (size_t)-1;
        size_t match_count = 0U;

        for (index = 0U; index < timeline->record_count; ++index) {
            record = &timeline->records[index];
            if (same_conversation(record, event) && record->has_opened &&
                !record->has_closed) {
                match_index = index;
                ++match_count;
            }
        }

        if (match_count == 1U) {
            record = &timeline->records[match_index];
            record->has_closed = TRUE;
            record->closed_timestamp = event->timestamp;
            record->close_result = event->result;
            return COMTRACE_DDE_TIMELINE_SUCCESS;
        }
        if (match_count == 0U) {
            increment_count(&timeline->unmatched_closes);
        } else {
            increment_count(&timeline->ambiguous_closes);
        }
    }

    if (event->type == COMTRACE_DDE_TIMELINE_TRANSACTION_COMPLETED) {
        size_t match_index = (size_t)-1;
        size_t match_count = 0U;

        for (index = 0U; index < timeline->record_count; ++index) {
            record = &timeline->records[index];
            if (record->has_start && !record->has_completion &&
                same_transaction(record, event)) {
                match_index = index;
                ++match_count;
            }
        }

        if (match_count == 1U) {
            record = &timeline->records[match_index];
            record->has_completion = TRUE;
            record->completion_timestamp = event->timestamp;
            record->result = event->result;
            record->data_size = event->data_size;
            record->succeeded = event->succeeded;
            if (transaction_metadata_mismatch(record, event)) {
                record->metadata_mismatch = TRUE;
                increment_count(&timeline->metadata_mismatches);
            }
            copy_transaction_fields(record, event);
            if (event->timestamp >= record->start_timestamp) {
                record->duration_ticks = event->timestamp - record->start_timestamp;
                record->has_duration = TRUE;
            } else {
                record->invalid_duration_order = TRUE;
                increment_count(&timeline->invalid_duration_orders);
            }
            return COMTRACE_DDE_TIMELINE_SUCCESS;
        }
        if (match_count > 1U) {
            increment_count(&timeline->ambiguous_completions);
        }
    }

    if (!ensure_record_capacity(timeline)) {
        return COMTRACE_DDE_TIMELINE_ALLOCATION_FAILED;
    }

    record = &timeline->records[timeline->record_count];
    ZeroMemory(record, sizeof(*record));
    record->process_id = event->process_id;
    record->thread_id = event->thread_id;
    record->dde_instance_id = event->dde_instance_id;
    record->conversation_id = event->conversation_id;
    record->conversation_correlation_id = event->conversation_correlation_id;
    record->transaction_instance_id = event->transaction_instance_id;
    record->transaction_correlation_id = event->transaction_correlation_id;
    record->dde_transaction_id = event->dde_transaction_id;
    record->transaction_type = event->transaction_type;
    record->format = event->format;
    record->asynchronous = event->asynchronous;
    record->role = event->role;
    if (!copy_text(record->item, _countof(record->item), event->item) ||
        !copy_text(record->service, (sizeof(record->service) / sizeof(record->service[0])), event->service) ||
        !copy_text(record->topic, (sizeof(record->topic) / sizeof(record->topic[0])), event->topic)) {
        return COMTRACE_DDE_TIMELINE_INVALID_ARGUMENT;
    }

    switch (event->type) {
    case COMTRACE_DDE_TIMELINE_CONVERSATION_OPENED:
        record->has_opened = TRUE;
        record->opened_timestamp = event->timestamp;
        break;
    case COMTRACE_DDE_TIMELINE_CONVERSATION_CLOSED:
        record->has_closed = TRUE;
        record->closed_timestamp = event->timestamp;
        record->close_result = event->result;
        break;
    case COMTRACE_DDE_TIMELINE_TRANSACTION_STARTED:
        record->has_start = TRUE;
        record->start_timestamp = event->timestamp;
        if (event->transaction_correlation_id == 0ULL) {
            increment_count(&timeline->missing_transaction_correlation_ids);
        }
        break;
    case COMTRACE_DDE_TIMELINE_TRANSACTION_COMPLETED:
        record->has_completion = TRUE;
        record->completion_timestamp = event->timestamp;
        record->result = event->result;
        record->data_size = event->data_size;
        record->succeeded = event->succeeded;
        increment_count(&timeline->unmatched_completions);
        break;
    case COMTRACE_DDE_TIMELINE_CALLBACK_RECEIVED:
        record->has_callback = TRUE;
        record->callback_timestamp = event->timestamp;
        record->data_size = event->data_size;
        break;
    default:
        return COMTRACE_DDE_TIMELINE_INVALID_ARGUMENT;
    }

    ++timeline->record_count;
    return COMTRACE_DDE_TIMELINE_SUCCESS;
}

COMTRACE_DDE_TIMELINE_STATUS comtrace_dde_timeline_correlate(
    COMTRACE_DDE_TIMELINE *timeline
)
{
    size_t *match_counts;
    size_t *candidate_indices;
    size_t left_index;
    size_t right_index;

    if (timeline == NULL) {
        return COMTRACE_DDE_TIMELINE_INVALID_ARGUMENT;
    }

    free(timeline->exchanges);
    timeline->exchanges = NULL;
    timeline->exchange_count = 0U;
    timeline->exchange_capacity = 0U;
    timeline->ambiguous_correlations = 0ULL;

    if (timeline->record_count < 2U) {
        return COMTRACE_DDE_TIMELINE_SUCCESS;
    }

    if (timeline->record_count > (size_t)-1 / sizeof(*match_counts)) {
        return COMTRACE_DDE_TIMELINE_ALLOCATION_FAILED;
    }

    match_counts = (size_t *)calloc(timeline->record_count, sizeof(*match_counts));
    candidate_indices = (size_t *)calloc(timeline->record_count, sizeof(*candidate_indices));
    if (match_counts == NULL || candidate_indices == NULL) {
        free(match_counts);
        free(candidate_indices);
        return COMTRACE_DDE_TIMELINE_ALLOCATION_FAILED;
    }

    for (left_index = 0U; left_index < timeline->record_count; ++left_index) {
        if (!timeline->records[left_index].has_start &&
            !timeline->records[left_index].has_callback) {
            continue;
        }
        for (right_index = left_index + 1U;
             right_index < timeline->record_count;
             ++right_index) {
            if ((!timeline->records[right_index].has_start &&
                 !timeline->records[right_index].has_callback) ||
                !records_can_correlate(
                    &timeline->records[left_index],
                    &timeline->records[right_index])) {
                continue;
            }
            ++match_counts[left_index];
            ++match_counts[right_index];
            candidate_indices[left_index] = right_index;
            candidate_indices[right_index] = left_index;
        }
    }

    for (left_index = 0U; left_index < timeline->record_count; ++left_index) {
        if (match_counts[left_index] > 1U) {
            increment_count(&timeline->ambiguous_correlations);
        }
        if (match_counts[left_index] != 1U ||
            candidate_indices[left_index] <= left_index ||
            match_counts[candidate_indices[left_index]] != 1U) {
            continue;
        }

        if (!ensure_exchange_capacity(timeline)) {
            free(match_counts);
            free(candidate_indices);
            return COMTRACE_DDE_TIMELINE_ALLOCATION_FAILED;
        }

        {
            const COMTRACE_DDE_TIMELINE_RECORD *left =
                &timeline->records[left_index];
            COMTRACE_DDE_EXCHANGE *exchange =
                &timeline->exchanges[timeline->exchange_count];

            if (left->role == COMTRACE_DDE_ROLE_CLIENT) {
                exchange->client_record_index = left_index;
                exchange->server_record_index = candidate_indices[left_index];
            } else {
                exchange->client_record_index = candidate_indices[left_index];
                exchange->server_record_index = left_index;
            }
            exchange->transaction_correlation_id = left->transaction_correlation_id;
            ++timeline->exchange_count;
        }
    }

    free(match_counts);
    free(candidate_indices);
    return COMTRACE_DDE_TIMELINE_SUCCESS;
}

size_t comtrace_dde_timeline_count(const COMTRACE_DDE_TIMELINE *timeline)
{
    return timeline == NULL ? 0U : timeline->record_count;
}

size_t comtrace_dde_timeline_exchange_count(const COMTRACE_DDE_TIMELINE *timeline)
{
    return timeline == NULL ? 0U : timeline->exchange_count;
}

const COMTRACE_DDE_TIMELINE_RECORD *comtrace_dde_timeline_get(
    const COMTRACE_DDE_TIMELINE *timeline,
    size_t index
)
{
    if (timeline == NULL || index >= timeline->record_count) {
        return NULL;
    }

    return &timeline->records[index];
}

const COMTRACE_DDE_EXCHANGE *comtrace_dde_timeline_get_exchange(
    const COMTRACE_DDE_TIMELINE *timeline,
    size_t index
)
{
    if (timeline == NULL || index >= timeline->exchange_count) {
        return NULL;
    }

    return &timeline->exchanges[index];
}
