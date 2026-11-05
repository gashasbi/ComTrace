#include "archive_reader.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define ARCHIVE_HEADER_SIZE 24ULL
#define ARCHIVE_RECORD_HEADER_SIZE 24ULL
#define ARCHIVE_EVENT_HEADER_SIZE 80ULL
#define ARCHIVE_EVENT_METADATA_SIZE 88ULL
#define ARCHIVE_FOOTER_SIZE 124ULL
#define ARCHIVE_RECORD_TYPE_EVENT 1U

static const BYTE archive_magic[8] = {
    'C', 'T', 'R', 'A', 'C', 'E', 'A', '1'
};

static const BYTE footer_magic[8] = {
    'C', 'T', 'R', 'F', 'T', 'R', '0', '1'
};

static USHORT decode_u16(const BYTE *data)
{
    return (USHORT)(
        (USHORT)data[0] |
        ((USHORT)data[1] << 8U)
    );
}

static ULONG decode_u32(const BYTE *data)
{
    return
        (ULONG)data[0] |
        ((ULONG)data[1] << 8U) |
        ((ULONG)data[2] << 16U) |
        ((ULONG)data[3] << 24U);
}

static ULONGLONG decode_u64(const BYTE *data)
{
    ULONGLONG value = 0ULL;
    unsigned int index;

    for (index = 0U; index < 8U; ++index) {
        value |= (ULONGLONG)data[index] << (index * 8U);
    }

    return value;
}

static COMTRACE_ARCHIVE_READER_STATUS set_failure(
    COMTRACE_ARCHIVE_READER *reader,
    COMTRACE_ARCHIVE_READER_STATUS status,
    DWORD error
)
{
    if (reader != NULL) {
        reader->status = status;
        reader->win32_error = error;
    }

    return status;
}

static BOOL read_exact(
    COMTRACE_ARCHIVE_READER *reader,
    BYTE *buffer,
    size_t size
)
{
    size_t total = 0U;

    while (total < size) {
        DWORD requested = size - total > (size_t)MAXDWORD
            ? MAXDWORD
            : (DWORD)(size - total);
        DWORD received = 0U;

        if (!ReadFile(reader->file, buffer + total, requested, &received, NULL)) {
            reader->win32_error = GetLastError();
            reader->status = COMTRACE_ARCHIVE_READER_READ_FAILED;
            return FALSE;
        }

        if (received == 0U || received > requested) {
            reader->status = COMTRACE_ARCHIVE_READER_TRUNCATED;
            reader->win32_error = ERROR_HANDLE_EOF;
            return FALSE;
        }

        total += (size_t)received;
        reader->offset += (ULONGLONG)received;
    }

    return TRUE;
}

static void decode_event_header(
    EVENT_HEADER *header,
    const BYTE *data
)
{
    ZeroMemory(header, sizeof(*header));
    header->Size = decode_u16(data);
    header->HeaderType = decode_u16(data + 2U);
    header->Flags = decode_u16(data + 4U);
    header->EventProperty = decode_u16(data + 6U);
    header->ThreadId = decode_u32(data + 8U);
    header->ProcessId = decode_u32(data + 12U);
    header->TimeStamp.QuadPart = (LONGLONG)decode_u64(data + 16U);
    memcpy(&header->ProviderId, data + 24U, sizeof(header->ProviderId));
    header->EventDescriptor.Id = decode_u16(data + 40U);
    header->EventDescriptor.Version = data[42U];
    header->EventDescriptor.Channel = data[43U];
    header->EventDescriptor.Level = data[44U];
    header->EventDescriptor.Opcode = data[45U];
    header->EventDescriptor.Task = decode_u16(data + 46U);
    header->EventDescriptor.Keyword = decode_u64(data + 48U);
    header->KernelTime = decode_u32(data + 56U);
    header->UserTime = decode_u32(data + 60U);
    memcpy(&header->ActivityId, data + 64U, sizeof(header->ActivityId));
}

static void decode_footer(
    COMTRACE_ARCHIVE_FOOTER *footer,
    const BYTE *data
)
{
    footer->records_written = decode_u64(data + 16U);
    footer->events_lost = decode_u32(data + 24U);
    footer->buffers_lost = decode_u32(data + 28U);
    footer->buffers_written = decode_u32(data + 32U);
    footer->buffers_read = decode_u32(data + 36U);
    footer->process_trace_status = decode_u32(data + 40U);
    footer->completion_marker = decode_u32(data + 44U);
    footer->trace_clock_type = decode_u32(data + 48U);
    footer->pointer_size = decode_u32(data + 52U);
    footer->os_version = decode_u32(data + 56U);
    footer->provider_version = decode_u32(data + 60U);
    footer->number_of_processors = decode_u32(data + 64U);
    footer->buffer_size = decode_u32(data + 68U);
    footer->timer_resolution = decode_u32(data + 72U);
    footer->maximum_file_size = decode_u32(data + 76U);
    footer->log_file_mode = decode_u32(data + 80U);
    footer->start_buffers = decode_u32(data + 84U);
    footer->cpu_speed_mhz = decode_u32(data + 88U);
    footer->start_time = decode_u64(data + 92U);
    footer->end_time = decode_u64(data + 100U);
    footer->boot_time = decode_u64(data + 108U);
    footer->perf_frequency = decode_u64(data + 116U);
}

static COMTRACE_ARCHIVE_READER_STATUS finish_recovery(
    COMTRACE_ARCHIVE_READER *reader,
    COMTRACE_ARCHIVE_RECOVERY_STOP_REASON reason,
    ULONGLONG stop_offset,
    BOOL footer_valid,
    BOOL archive_complete
)
{
    reader->recovery_info.records_recovered = reader->records_read;
    reader->recovery_info.stop_offset = stop_offset;
    reader->recovery_info.footer_valid = footer_valid;
    reader->recovery_info.archive_complete = archive_complete;
    reader->recovery_info.stop_reason = reason;
    reader->footer_read = TRUE;
    reader->status = COMTRACE_ARCHIVE_READER_SUCCESS;
    return COMTRACE_ARCHIVE_READER_END;
}

static COMTRACE_ARCHIVE_READER_STATUS read_footer(
    COMTRACE_ARCHIVE_READER *reader,
    const BYTE *prefix
)
{
    BYTE footer_data[ARCHIVE_FOOTER_SIZE];
    ULONGLONG remaining;
    ULONGLONG footer_offset = reader->offset - 8ULL;

    memcpy(footer_data, prefix, 8U);
    if (reader->file_size - reader->offset < ARCHIVE_FOOTER_SIZE - 8ULL) {
        if (reader->recovery_mode) {
            return finish_recovery(
                reader,
                COMTRACE_ARCHIVE_RECOVERY_STOP_FOOTER_TRUNCATED,
                footer_offset,
                FALSE,
                FALSE
            );
        }

        return set_failure(
            reader,
            COMTRACE_ARCHIVE_READER_TRUNCATED,
            ERROR_HANDLE_EOF
        );
    }

    if (!read_exact(
            reader,
            footer_data + 8U,
            (size_t)ARCHIVE_FOOTER_SIZE - 8U)) {
        return reader->status;
    }

    if (decode_u32(footer_data + 8U) != 1U ||
        decode_u32(footer_data + 12U) != ARCHIVE_FOOTER_SIZE) {
        if (reader->recovery_mode) {
            return finish_recovery(
                reader,
                COMTRACE_ARCHIVE_RECOVERY_STOP_FOOTER_INVALID,
                footer_offset,
                FALSE,
                FALSE
            );
        }

        return set_failure(
            reader,
            COMTRACE_ARCHIVE_READER_INVALID_FORMAT,
            ERROR_INVALID_DATA
        );
    }

    decode_footer(&reader->footer, footer_data);

    if (reader->footer.records_written != reader->records_read ||
        reader->footer.process_trace_status != ERROR_SUCCESS ||
        reader->footer.completion_marker != 1U) {
        if (reader->recovery_mode) {
            return finish_recovery(
                reader,
                COMTRACE_ARCHIVE_RECOVERY_STOP_FOOTER_INVALID,
                footer_offset,
                FALSE,
                FALSE
            );
        }

        return set_failure(
            reader,
            COMTRACE_ARCHIVE_READER_INVALID_FORMAT,
            ERROR_INVALID_DATA
        );
    }

    remaining = reader->file_size - reader->offset;
    if (remaining != 0ULL) {
        if (reader->recovery_mode) {
            return finish_recovery(
                reader,
                COMTRACE_ARCHIVE_RECOVERY_STOP_TRAILING_DATA,
                reader->offset,
                TRUE,
                FALSE
            );
        }

        return set_failure(
            reader,
            COMTRACE_ARCHIVE_READER_INVALID_FORMAT,
            ERROR_INVALID_DATA
        );
    }

    reader->footer_read = TRUE;
    reader->status = COMTRACE_ARCHIVE_READER_SUCCESS;

    if (reader->recovery_mode) {
        return finish_recovery(
            reader,
            COMTRACE_ARCHIVE_RECOVERY_STOP_NONE,
            reader->offset,
            TRUE,
            TRUE
        );
    }

    return COMTRACE_ARCHIVE_READER_END;
}

static COMTRACE_ARCHIVE_READER_STATUS archive_reader_open_internal(
    COMTRACE_ARCHIVE_READER *reader,
    const wchar_t *path,
    BOOL recovery_mode
)
{
    LARGE_INTEGER size;
    BYTE header[ARCHIVE_HEADER_SIZE];
    COMTRACE_ARCHIVE_READER_STATUS status;
    DWORD error_code;

    if (reader == NULL) {
        return COMTRACE_ARCHIVE_READER_INVALID_ARGUMENT;
    }

    ZeroMemory(reader, sizeof(*reader));
    reader->file = INVALID_HANDLE_VALUE;
    reader->status = COMTRACE_ARCHIVE_READER_SUCCESS;
    reader->recovery_mode = recovery_mode;

    if (path == NULL || path[0] == L'\0') {
        return set_failure(
            reader,
            COMTRACE_ARCHIVE_READER_INVALID_ARGUMENT,
            ERROR_INVALID_PARAMETER
        );
    }

    reader->file = CreateFileW(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        NULL
    );

    if (reader->file == INVALID_HANDLE_VALUE) {
        return set_failure(
            reader,
            COMTRACE_ARCHIVE_READER_OPEN_FAILED,
            GetLastError()
        );
    }

    if (!GetFileSizeEx(reader->file, &size)) {
        error_code = GetLastError();
        status = set_failure(
            reader,
            COMTRACE_ARCHIVE_READER_READ_FAILED,
            error_code
        );
        goto failure;
    }

    if (size.QuadPart < 0) {
        status = set_failure(
            reader,
            COMTRACE_ARCHIVE_READER_READ_FAILED,
            ERROR_FILE_INVALID
        );
        goto failure;
    }

    reader->file_size = (ULONGLONG)size.QuadPart;

    if (reader->file_size < ARCHIVE_HEADER_SIZE ||
        (!recovery_mode &&
         reader->file_size < ARCHIVE_HEADER_SIZE + ARCHIVE_FOOTER_SIZE)) {
        status = set_failure(
            reader,
            COMTRACE_ARCHIVE_READER_TRUNCATED,
            ERROR_HANDLE_EOF
        );
        goto failure;
    }

    if (!read_exact(reader, header, sizeof(header))) {
        status = reader->status;
        goto failure;
    }

    if (memcmp(header, archive_magic, sizeof(archive_magic)) != 0 ||
        decode_u32(header + 8U) != 1U ||
        decode_u32(header + 12U) != ARCHIVE_HEADER_SIZE ||
        decode_u32(header + 16U) != 1U ||
        decode_u32(header + 20U) != 0U) {
        status = set_failure(
            reader,
            COMTRACE_ARCHIVE_READER_INVALID_FORMAT,
            ERROR_INVALID_DATA
        );
        goto failure;
    }

    return COMTRACE_ARCHIVE_READER_SUCCESS;

failure:
    (void)CloseHandle(reader->file);
    reader->file = INVALID_HANDLE_VALUE;
    return status;
}

COMTRACE_ARCHIVE_READER_STATUS comtrace_archive_reader_open(
    COMTRACE_ARCHIVE_READER *reader,
    const wchar_t *path
)
{
    return archive_reader_open_internal(reader, path, FALSE);
}

COMTRACE_ARCHIVE_READER_STATUS comtrace_archive_reader_open_recovery(
    COMTRACE_ARCHIVE_READER *reader,
    const wchar_t *path
)
{
    return archive_reader_open_internal(reader, path, TRUE);
}

const COMTRACE_ARCHIVE_RECOVERY_INFO *
comtrace_archive_reader_get_recovery_info(
    const COMTRACE_ARCHIVE_READER *reader
)
{
    if (reader == NULL || !reader->recovery_mode) {
        return NULL;
    }

    return &reader->recovery_info;
}

COMTRACE_ARCHIVE_READER_STATUS comtrace_archive_reader_next(
    COMTRACE_ARCHIVE_READER *reader,
    COMTRACE_ARCHIVE_EVENT *event
)
{
    BYTE frame[ARCHIVE_RECORD_HEADER_SIZE];
    BYTE *payload = NULL;
    ULONGLONG record_offset;
    ULONGLONG sequence;
    ULONGLONG payload_size;
    ULONGLONG remaining;
    ULONG record_type;
    ULONG record_flags;
    USHORT extended_count;
    USHORT user_data_length;
    size_t cursor;
    USHORT index;

    if (reader == NULL || event == NULL ||
        reader->file == NULL || reader->file == INVALID_HANDLE_VALUE) {
        return COMTRACE_ARCHIVE_READER_INVALID_ARGUMENT;
    }

    ZeroMemory(event, sizeof(*event));

    if (reader->status != COMTRACE_ARCHIVE_READER_SUCCESS) {
        return reader->status;
    }

    if (reader->footer_read) {
        return COMTRACE_ARCHIVE_READER_END;
    }

    record_offset = reader->offset;
    remaining = reader->file_size - reader->offset;

    if (remaining < 8ULL) {
        if (reader->recovery_mode) {
            if (remaining == 0ULL) {
                return finish_recovery(
                    reader,
                    COMTRACE_ARCHIVE_RECOVERY_STOP_FOOTER_MISSING,
                    record_offset,
                    FALSE,
                    FALSE
                );
            }

            if (!read_exact(reader, frame, (size_t)remaining)) {
                return reader->status;
            }

            if (memcmp(frame, footer_magic, (size_t)remaining) == 0) {
                return finish_recovery(
                    reader,
                    COMTRACE_ARCHIVE_RECOVERY_STOP_FOOTER_TRUNCATED,
                    record_offset,
                    FALSE,
                    FALSE
                );
            }

            return finish_recovery(
                reader,
                COMTRACE_ARCHIVE_RECOVERY_STOP_RECORD_HEADER_TRUNCATED,
                record_offset,
                FALSE,
                FALSE
            );
        }

        return set_failure(
            reader,
            COMTRACE_ARCHIVE_READER_TRUNCATED,
            ERROR_HANDLE_EOF
        );
    }

    if (!read_exact(reader, frame, 8U)) {
        return reader->status;
    }

    if (memcmp(frame, footer_magic, sizeof(footer_magic)) == 0) {
        return read_footer(reader, frame);
    }

    if (reader->recovery_mode) {
        remaining = reader->file_size - reader->offset;

        if (remaining < ARCHIVE_RECORD_HEADER_SIZE - 8ULL) {
            return finish_recovery(
                reader,
                COMTRACE_ARCHIVE_RECOVERY_STOP_RECORD_HEADER_TRUNCATED,
                record_offset,
                FALSE,
                FALSE
            );
        }
    } else if (remaining < ARCHIVE_RECORD_HEADER_SIZE + ARCHIVE_FOOTER_SIZE - 8ULL) {
        return set_failure(
            reader,
            COMTRACE_ARCHIVE_READER_TRUNCATED,
            ERROR_HANDLE_EOF
        );
    }

    if (!read_exact(
            reader,
            frame + 8U,
            sizeof(frame) - 8U)) {
        return reader->status;
    }

    record_type = decode_u32(frame);
    record_flags = decode_u32(frame + 4U);
    sequence = decode_u64(frame + 8U);
    payload_size = decode_u64(frame + 16U);

    if (record_type != ARCHIVE_RECORD_TYPE_EVENT ||
        record_flags != 0U ||
        reader->records_read == UINT64_MAX ||
        sequence != reader->records_read + 1ULL ||
        payload_size < ARCHIVE_EVENT_METADATA_SIZE ||
        payload_size > (ULONGLONG)SIZE_MAX) {
        if (reader->recovery_mode) {
            return finish_recovery(
                reader,
                COMTRACE_ARCHIVE_RECOVERY_STOP_RECORD_INVALID,
                record_offset,
                FALSE,
                FALSE
            );
        }

        return set_failure(
            reader,
            COMTRACE_ARCHIVE_READER_INVALID_FORMAT,
            ERROR_INVALID_DATA
        );
    }

    remaining = reader->file_size - reader->offset;

    if (reader->recovery_mode) {
        if (payload_size > remaining) {
            return finish_recovery(
                reader,
                COMTRACE_ARCHIVE_RECOVERY_STOP_RECORD_PAYLOAD_TRUNCATED,
                record_offset,
                FALSE,
                FALSE
            );
        }
    } else if (remaining < ARCHIVE_FOOTER_SIZE ||
               payload_size > remaining - ARCHIVE_FOOTER_SIZE) {
        return set_failure(
            reader,
            COMTRACE_ARCHIVE_READER_TRUNCATED,
            ERROR_HANDLE_EOF
        );
    }

    payload = (BYTE *)malloc((size_t)payload_size);
    if (payload == NULL) {
        return set_failure(
            reader,
            COMTRACE_ARCHIVE_READER_ALLOCATION_FAILED,
            ERROR_NOT_ENOUGH_MEMORY
        );
    }

    if (!read_exact(reader, payload, (size_t)payload_size)) {
        free(payload);
        return reader->status;
    }

    decode_event_header(&event->record.EventHeader, payload);
    event->record.BufferContext.ProcessorNumber = payload[80U];
    event->record.BufferContext.Alignment = payload[81U];
    event->record.BufferContext.LoggerId = decode_u16(payload + 82U);
    extended_count = decode_u16(payload + 84U);
    user_data_length = decode_u16(payload + 86U);

    cursor = (size_t)ARCHIVE_EVENT_METADATA_SIZE;
    if ((size_t)extended_count >
        ((size_t)payload_size - cursor) / 8U) {
        free(payload);

        if (reader->recovery_mode) {
            ZeroMemory(event, sizeof(*event));
            return finish_recovery(
                reader,
                COMTRACE_ARCHIVE_RECOVERY_STOP_RECORD_INVALID,
                record_offset,
                FALSE,
                FALSE
            );
        }

        return set_failure(
            reader,
            COMTRACE_ARCHIVE_READER_INVALID_FORMAT,
            ERROR_INVALID_DATA
        );
    }

    if (extended_count != 0U) {
        event->extended_data = (EVENT_HEADER_EXTENDED_DATA_ITEM *)calloc(
            (size_t)extended_count,
            sizeof(*event->extended_data)
        );

        if (event->extended_data == NULL) {
            free(payload);
            return set_failure(
                reader,
                COMTRACE_ARCHIVE_READER_ALLOCATION_FAILED,
                ERROR_NOT_ENOUGH_MEMORY
            );
        }
    }

    for (index = 0U; index < extended_count; ++index) {
        EVENT_HEADER_EXTENDED_DATA_ITEM *item = &event->extended_data[index];
        USHORT data_size;

        item->Reserved1 = decode_u16(payload + cursor);
        item->ExtType = decode_u16(payload + cursor + 2U);
        data_size = decode_u16(payload + cursor + 6U);
        memcpy((BYTE *)item + 4U, payload + cursor + 4U, 4U);
        cursor += 8U;

        if ((size_t)data_size > (size_t)payload_size - cursor) {
            free(event->extended_data);
            free(payload);
            ZeroMemory(event, sizeof(*event));

            if (reader->recovery_mode) {
                return finish_recovery(
                    reader,
                    COMTRACE_ARCHIVE_RECOVERY_STOP_RECORD_INVALID,
                    record_offset,
                    FALSE,
                    FALSE
                );
            }

            return set_failure(
                reader,
                COMTRACE_ARCHIVE_READER_INVALID_FORMAT,
                ERROR_INVALID_DATA
            );
        }

        item->DataSize = data_size;
        item->DataPtr = data_size == 0U
            ? 0ULL
            : (ULONGLONG)(ULONG_PTR)(payload + cursor);
        cursor += (size_t)data_size;
    }

    if ((size_t)user_data_length != (size_t)payload_size - cursor) {
        free(event->extended_data);
        free(payload);
        ZeroMemory(event, sizeof(*event));

        if (reader->recovery_mode) {
            return finish_recovery(
                reader,
                COMTRACE_ARCHIVE_RECOVERY_STOP_RECORD_INVALID,
                record_offset,
                FALSE,
                FALSE
            );
        }

        return set_failure(
            reader,
            COMTRACE_ARCHIVE_READER_INVALID_FORMAT,
            ERROR_INVALID_DATA
        );
    }

    event->payload = payload;
    event->record.ExtendedDataCount = extended_count;
    event->record.ExtendedData = event->extended_data;
    event->record.UserDataLength = user_data_length;
    event->record.UserData = user_data_length == 0U
        ? NULL
        : payload + cursor;
    event->record.UserContext = NULL;

    ++reader->records_read;

    if (reader->recovery_mode) {
        reader->recovery_info.records_recovered = reader->records_read;
    }

    reader->status = COMTRACE_ARCHIVE_READER_SUCCESS;
    return COMTRACE_ARCHIVE_READER_SUCCESS;
}
void comtrace_archive_event_destroy(COMTRACE_ARCHIVE_EVENT *event)
{
    if (event == NULL) {
        return;
    }

    free(event->extended_data);
    free(event->payload);
    ZeroMemory(event, sizeof(*event));
}

void comtrace_archive_reader_close(COMTRACE_ARCHIVE_READER *reader)
{
    if (reader == NULL) {
        return;
    }

    if (reader->file != NULL && reader->file != INVALID_HANDLE_VALUE) {
        (void)CloseHandle(reader->file);
    }

    ZeroMemory(reader, sizeof(*reader));
    reader->file = INVALID_HANDLE_VALUE;
}
