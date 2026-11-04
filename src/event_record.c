#include "event_record.h"

#include <stdlib.h>
#include <string.h>

static void release_owned_record(COMTRACE_OWNED_EVENT_RECORD *record)
{
    USHORT index;

    if (record == NULL) {
        return;
    }

    if (record->extended_data != NULL) {
        for (index = 0U; index < record->extended_data_count; ++index) {
            if (record->extended_data[index].DataPtr != 0ULL) {
                free((void *)(ULONG_PTR)record->extended_data[index].DataPtr);
            }
        }
    }

    free(record->extended_data);
    free(record->user_data);
    free(record);
}

COMTRACE_EVENT_RECORD_STATUS comtrace_event_record_copy(
    const EVENT_RECORD *source,
    COMTRACE_OWNED_EVENT_RECORD **destination
)
{
    COMTRACE_OWNED_EVENT_RECORD *owned;
    USHORT index;

    if (destination == NULL) {
        return COMTRACE_EVENT_RECORD_INVALID_ARGUMENT;
    }

    *destination = NULL;

    if (source == NULL) {
        return COMTRACE_EVENT_RECORD_INVALID_ARGUMENT;
    }

    if ((source->UserDataLength != 0U && source->UserData == NULL) ||
        (source->ExtendedDataCount != 0U && source->ExtendedData == NULL)) {
        return COMTRACE_EVENT_RECORD_INVALID_SOURCE;
    }

    owned = (COMTRACE_OWNED_EVENT_RECORD *)calloc(1U, sizeof(*owned));

    if (owned == NULL) {
        return COMTRACE_EVENT_RECORD_ALLOCATION_FAILED;
    }

    owned->record = *source;
    owned->record.UserContext = NULL;
    owned->record.UserData = NULL;
    owned->record.ExtendedData = NULL;

    if (source->UserDataLength != 0U) {
        owned->user_data = (BYTE *)malloc((size_t)source->UserDataLength);

        if (owned->user_data == NULL) {
            release_owned_record(owned);
            return COMTRACE_EVENT_RECORD_ALLOCATION_FAILED;
        }

        memcpy(
            owned->user_data,
            source->UserData,
            (size_t)source->UserDataLength
        );

        owned->record.UserData = owned->user_data;
    }

    if (source->ExtendedDataCount != 0U) {
        owned->extended_data = (EVENT_HEADER_EXTENDED_DATA_ITEM *)calloc(
            (size_t)source->ExtendedDataCount,
            sizeof(*owned->extended_data)
        );

        if (owned->extended_data == NULL) {
            release_owned_record(owned);
            return COMTRACE_EVENT_RECORD_ALLOCATION_FAILED;
        }

        owned->extended_data_count = source->ExtendedDataCount;

        for (index = 0U; index < source->ExtendedDataCount; ++index) {
            const BYTE *source_data;
            BYTE *data_copy;

            owned->extended_data[index] = source->ExtendedData[index];
            owned->extended_data[index].DataPtr = 0ULL;

            if (source->ExtendedData[index].DataSize == 0U) {
                continue;
            }

            source_data = (const BYTE *)(ULONG_PTR)
                source->ExtendedData[index].DataPtr;

            if (source_data == NULL) {
                release_owned_record(owned);
                return COMTRACE_EVENT_RECORD_INVALID_SOURCE;
            }

            data_copy = (BYTE *)malloc(
                (size_t)source->ExtendedData[index].DataSize
            );

            if (data_copy == NULL) {
                release_owned_record(owned);
                return COMTRACE_EVENT_RECORD_ALLOCATION_FAILED;
            }

            memcpy(
                data_copy,
                source_data,
                (size_t)source->ExtendedData[index].DataSize
            );

            owned->extended_data[index].DataPtr =
                (ULONGLONG)(ULONG_PTR)data_copy;
        }

        owned->record.ExtendedData = owned->extended_data;
    }

    *destination = owned;
    return COMTRACE_EVENT_RECORD_SUCCESS;
}

void comtrace_event_record_destroy(COMTRACE_OWNED_EVENT_RECORD *record)
{
    release_owned_record(record);
}