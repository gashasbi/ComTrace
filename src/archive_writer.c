#include "archive_writer.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define ARCHIVE_HEADER_SIZE 24U
#define ARCHIVE_RECORD_HEADER_SIZE 24U
#define ARCHIVE_EVENT_HEADER_SIZE 80U
#define ARCHIVE_EVENT_METADATA_SIZE 88U
#define ARCHIVE_FOOTER_SIZE 124U
#define ARCHIVE_RECORD_TYPE_EVENT 1U

_Static_assert(
    offsetof(EVENT_HEADER_EXTENDED_DATA_ITEM, DataPtr) == 8U,
    "Unexpected extended data descriptor layout"
);

static const BYTE archive_magic[8] = {
    'C', 'T', 'R', 'A', 'C', 'E', 'A', '1'
};

static const BYTE footer_magic[8] = {
    'C', 'T', 'R', 'F', 'T', 'R', '0', '1'
};

static COMTRACE_ARCHIVE_STATUS set_failure(
    COMTRACE_ARCHIVE_WRITER *writer,
    COMTRACE_ARCHIVE_STATUS status,
    DWORD win32_error
)
{
    if (writer != NULL) {
        writer->status = status;
        writer->win32_error = win32_error;
    }

    return status;
}

static int writer_has_open_file(const COMTRACE_ARCHIVE_WRITER *writer)
{
    return writer != NULL &&
        writer->file != NULL &&
        writer->file != INVALID_HANDLE_VALUE;
}
static COMTRACE_ARCHIVE_STATUS write_bytes(
    COMTRACE_ARCHIVE_WRITER *writer,
    const BYTE *data,
    size_t size
)
{
    if (!writer_has_open_file(writer) ||
        (size != 0U && data == NULL)) {
        return COMTRACE_ARCHIVE_INVALID_ARGUMENT;
    }

    while (size != 0U) {
        DWORD requested = size > (size_t)MAXDWORD
            ? MAXDWORD
            : (DWORD)size;
        DWORD written = 0U;

        if (!WriteFile(
                writer->file,
                data,
                requested,
                &written,
                NULL)) {
            return set_failure(
                writer,
                COMTRACE_ARCHIVE_WRITE_FAILED,
                GetLastError()
            );
        }

        if (written == 0U || written > requested) {
            return set_failure(
                writer,
                COMTRACE_ARCHIVE_WRITE_FAILED,
                ERROR_WRITE_FAULT
            );
        }

        data += written;
        size -= (size_t)written;
    }

    return COMTRACE_ARCHIVE_SUCCESS;
}

static COMTRACE_ARCHIVE_STATUS write_u8(
    COMTRACE_ARCHIVE_WRITER *writer,
    BYTE value
)
{
    return write_bytes(writer, &value, sizeof(value));
}

static COMTRACE_ARCHIVE_STATUS write_u16(
    COMTRACE_ARCHIVE_WRITER *writer,
    USHORT value
)
{
    BYTE bytes[2];

    bytes[0] = (BYTE)(value & 0xFFU);
    bytes[1] = (BYTE)((value >> 8U) & 0xFFU);

    return write_bytes(writer, bytes, sizeof(bytes));
}

static COMTRACE_ARCHIVE_STATUS write_u32(
    COMTRACE_ARCHIVE_WRITER *writer,
    ULONG value
)
{
    BYTE bytes[4];

    bytes[0] = (BYTE)(value & 0xFFUL);
    bytes[1] = (BYTE)((value >> 8U) & 0xFFUL);
    bytes[2] = (BYTE)((value >> 16U) & 0xFFUL);
    bytes[3] = (BYTE)((value >> 24U) & 0xFFUL);

    return write_bytes(writer, bytes, sizeof(bytes));
}

static COMTRACE_ARCHIVE_STATUS write_u64(
    COMTRACE_ARCHIVE_WRITER *writer,
    ULONGLONG value
)
{
    BYTE bytes[8];
    unsigned int index;

    for (index = 0U; index < 8U; ++index) {
        bytes[index] = (BYTE)((value >> (index * 8U)) & 0xFFULL);
    }

    return write_bytes(writer, bytes, sizeof(bytes));
}

static COMTRACE_ARCHIVE_STATUS write_file_header(
    COMTRACE_ARCHIVE_WRITER *writer
)
{
    COMTRACE_ARCHIVE_STATUS status;

    status = write_bytes(writer, archive_magic, sizeof(archive_magic));
    if (status != COMTRACE_ARCHIVE_SUCCESS) {
        return status;
    }

    status = write_u32(writer, 1U);
    if (status != COMTRACE_ARCHIVE_SUCCESS) {
        return status;
    }

    status = write_u32(writer, ARCHIVE_HEADER_SIZE);
    if (status != COMTRACE_ARCHIVE_SUCCESS) {
        return status;
    }

    status = write_u32(writer, 1U);
    if (status != COMTRACE_ARCHIVE_SUCCESS) {
        return status;
    }

    return write_u32(writer, 0U);
}

static COMTRACE_ARCHIVE_STATUS write_event_header(
    COMTRACE_ARCHIVE_WRITER *writer,
    const EVENT_HEADER *header
)
{
    COMTRACE_ARCHIVE_STATUS status;
    const EVENT_DESCRIPTOR *descriptor;

    descriptor = &header->EventDescriptor;

    status = write_u16(writer, header->Size);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u16(writer, header->HeaderType);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u16(writer, header->Flags);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u16(writer, header->EventProperty);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u32(writer, header->ThreadId);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u32(writer, header->ProcessId);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u64(
        writer,
        (ULONGLONG)header->TimeStamp.QuadPart
    );
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_bytes(
        writer,
        (const BYTE *)&header->ProviderId,
        sizeof(header->ProviderId)
    );
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u16(writer, descriptor->Id);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u8(writer, descriptor->Version);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u8(writer, descriptor->Channel);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u8(writer, descriptor->Level);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u8(writer, descriptor->Opcode);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u16(writer, descriptor->Task);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u64(writer, descriptor->Keyword);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u32(writer, header->KernelTime);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u32(writer, header->UserTime);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    return write_bytes(
        writer,
        (const BYTE *)&header->ActivityId,
        sizeof(header->ActivityId)
    );
}

COMTRACE_ARCHIVE_STATUS comtrace_archive_writer_open(
    COMTRACE_ARCHIVE_WRITER *writer,
    const wchar_t *output_path
)
{
    size_t path_length;
    DWORD attributes;
    DWORD error_code;
    COMTRACE_ARCHIVE_STATUS status;

    if (writer == NULL) {
        return COMTRACE_ARCHIVE_INVALID_ARGUMENT;
    }

    ZeroMemory(writer, sizeof(*writer));
    writer->file = INVALID_HANDLE_VALUE;
    writer->status = COMTRACE_ARCHIVE_SUCCESS;

    if (output_path == NULL || output_path[0] == L'\0') {
        return set_failure(
            writer,
            COMTRACE_ARCHIVE_INVALID_ARGUMENT,
            ERROR_INVALID_PARAMETER
        );
    }

    path_length = wcslen(output_path);

    if (path_length > SIZE_MAX / sizeof(wchar_t) - 9U) {
        return set_failure(
            writer,
            COMTRACE_ARCHIVE_SIZE_OVERFLOW,
            ERROR_ARITHMETIC_OVERFLOW
        );
    }

    writer->final_path = (wchar_t *)malloc(
        (path_length + 1U) * sizeof(wchar_t)
    );

    writer->partial_path = (wchar_t *)malloc(
        (path_length + 9U) * sizeof(wchar_t)
    );

    if (writer->final_path == NULL ||
        writer->partial_path == NULL) {
        return set_failure(
            writer,
            COMTRACE_ARCHIVE_ALLOCATION_FAILED,
            ERROR_NOT_ENOUGH_MEMORY
        );
    }

    memcpy(
        writer->final_path,
        output_path,
        (path_length + 1U) * sizeof(wchar_t)
    );

    memcpy(
        writer->partial_path,
        output_path,
        path_length * sizeof(wchar_t)
    );

    memcpy(
        writer->partial_path + path_length,
        L".partial",
        sizeof(L".partial")
    );

    attributes = GetFileAttributesW(writer->final_path);

    if (attributes != INVALID_FILE_ATTRIBUTES) {
        return set_failure(
            writer,
            COMTRACE_ARCHIVE_OUTPUT_EXISTS,
            ERROR_FILE_EXISTS
        );
    }

    error_code = GetLastError();

    if (error_code != ERROR_FILE_NOT_FOUND &&
        error_code != ERROR_PATH_NOT_FOUND) {
        return set_failure(
            writer,
            COMTRACE_ARCHIVE_OPEN_FAILED,
            error_code
        );
    }

    writer->file = CreateFileW(
        writer->partial_path,
        GENERIC_WRITE,
        0U,
        NULL,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
        NULL
    );

    if (writer->file == INVALID_HANDLE_VALUE) {
        error_code = GetLastError();

        return set_failure(
            writer,
            error_code == ERROR_FILE_EXISTS ||
                error_code == ERROR_ALREADY_EXISTS
                ? COMTRACE_ARCHIVE_OUTPUT_EXISTS
                : COMTRACE_ARCHIVE_OPEN_FAILED,
            error_code
        );
    }

    status = write_file_header(writer);

    if (status != COMTRACE_ARCHIVE_SUCCESS) {
        return status;
    }

    return COMTRACE_ARCHIVE_SUCCESS;
}

COMTRACE_ARCHIVE_STATUS comtrace_archive_writer_write_event(
    COMTRACE_ARCHIVE_WRITER *writer,
    const EVENT_RECORD *event_record
)
{
    ULONGLONG payload_size = ARCHIVE_EVENT_METADATA_SIZE;
    ULONGLONG sequence;
    USHORT index;
    COMTRACE_ARCHIVE_STATUS status;

    if (!writer_has_open_file(writer) ||
        event_record == NULL) {
        return COMTRACE_ARCHIVE_INVALID_ARGUMENT;
    }

    if (writer->status != COMTRACE_ARCHIVE_SUCCESS) {
        return writer->status;
    }

    if ((event_record->UserDataLength != 0U &&
         event_record->UserData == NULL) ||
        (event_record->ExtendedDataCount != 0U &&
         event_record->ExtendedData == NULL)) {
        return set_failure(
            writer,
            COMTRACE_ARCHIVE_INVALID_EVENT,
            ERROR_INVALID_DATA
        );
    }

    if (writer->records_written == UINT64_MAX) {
        return set_failure(
            writer,
            COMTRACE_ARCHIVE_SIZE_OVERFLOW,
            ERROR_ARITHMETIC_OVERFLOW
        );
    }

    payload_size += (ULONGLONG)event_record->UserDataLength;

    for (index = 0U;
         index < event_record->ExtendedDataCount;
         ++index) {
        const EVENT_HEADER_EXTENDED_DATA_ITEM *item =
            &event_record->ExtendedData[index];

        ULONGLONG item_size =
            8ULL + (ULONGLONG)item->DataSize;

        if (item->DataSize != 0U && item->DataPtr == 0ULL) {
            return set_failure(
                writer,
                COMTRACE_ARCHIVE_INVALID_EVENT,
                ERROR_INVALID_DATA
            );
        }

        if (payload_size > UINT64_MAX - item_size) {
            return set_failure(
                writer,
                COMTRACE_ARCHIVE_SIZE_OVERFLOW,
                ERROR_ARITHMETIC_OVERFLOW
            );
        }

        payload_size += item_size;
    }

    sequence = writer->records_written + 1ULL;

    status = write_u32(writer, ARCHIVE_RECORD_TYPE_EVENT);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u32(writer, 0U);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u64(writer, sequence);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u64(writer, payload_size);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_event_header(writer, &event_record->EventHeader);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u8(
        writer,
        event_record->BufferContext.ProcessorNumber
    );
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u8(
        writer,
        event_record->BufferContext.Alignment
    );
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u16(
        writer,
        event_record->BufferContext.LoggerId
    );
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u16(writer, event_record->ExtendedDataCount);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u16(writer, event_record->UserDataLength);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    for (index = 0U;
         index < event_record->ExtendedDataCount;
         ++index) {
        const EVENT_HEADER_EXTENDED_DATA_ITEM *item =
            &event_record->ExtendedData[index];

        status = write_bytes(
            writer,
            (const BYTE *)item,
            offsetof(EVENT_HEADER_EXTENDED_DATA_ITEM, DataPtr)
        );

        if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

        if (item->DataSize != 0U) {
            status = write_bytes(
                writer,
                (const BYTE *)(ULONG_PTR)item->DataPtr,
                (size_t)item->DataSize
            );

            if (status != COMTRACE_ARCHIVE_SUCCESS) return status;
        }
    }

    if (event_record->UserDataLength != 0U) {
        status = write_bytes(
            writer,
            (const BYTE *)event_record->UserData,
            (size_t)event_record->UserDataLength
        );

        if (status != COMTRACE_ARCHIVE_SUCCESS) return status;
    }

    ++writer->records_written;
    return COMTRACE_ARCHIVE_SUCCESS;
}

COMTRACE_ARCHIVE_STATUS comtrace_archive_writer_finalize(
    COMTRACE_ARCHIVE_WRITER *writer,
    const COMTRACE_ARCHIVE_SUMMARY *summary
)
{
    COMTRACE_ARCHIVE_STATUS status;
    BOOL close_result;

    if (!writer_has_open_file(writer) ||
        summary == NULL) {
        return COMTRACE_ARCHIVE_INVALID_ARGUMENT;
    }

    if (writer->status != COMTRACE_ARCHIVE_SUCCESS) {
        return writer->status;
    }

    if (summary->process_trace_status != ERROR_SUCCESS) {
        return set_failure(
            writer,
            COMTRACE_ARCHIVE_INVALID_ARGUMENT,
            summary->process_trace_status
        );
    }

    status = write_bytes(writer, footer_magic, sizeof(footer_magic));
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u32(writer, 1U);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u32(writer, ARCHIVE_FOOTER_SIZE);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u64(writer, writer->records_written);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u32(writer, summary->events_lost);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u32(writer, summary->buffers_lost);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u32(writer, summary->buffers_written);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u32(writer, summary->buffers_read);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u32(writer, summary->process_trace_status);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u32(writer, 1U);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u32(writer, summary->trace_clock_type);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u32(writer, summary->pointer_size);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u32(writer, summary->os_version);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u32(writer, summary->provider_version);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u32(writer, summary->number_of_processors);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u32(writer, summary->buffer_size);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u32(writer, summary->timer_resolution);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u32(writer, summary->maximum_file_size);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u32(writer, summary->log_file_mode);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u32(writer, summary->start_buffers);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u32(writer, summary->cpu_speed_mhz);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u64(writer, summary->start_time);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u64(writer, summary->end_time);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u64(writer, summary->boot_time);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;

    status = write_u64(writer, summary->perf_frequency);
    if (status != COMTRACE_ARCHIVE_SUCCESS) return status;
    if (!FlushFileBuffers(writer->file)) {
        return set_failure(
            writer,
            COMTRACE_ARCHIVE_FINALIZE_FAILED,
            GetLastError()
        );
    }

    close_result = CloseHandle(writer->file);
    writer->file = INVALID_HANDLE_VALUE;

    if (!close_result) {
        return set_failure(
            writer,
            COMTRACE_ARCHIVE_FINALIZE_FAILED,
            GetLastError()
        );
    }

    if (!MoveFileExW(
            writer->partial_path,
            writer->final_path,
            MOVEFILE_WRITE_THROUGH)) {
        return set_failure(
            writer,
            COMTRACE_ARCHIVE_FINALIZE_FAILED,
            GetLastError()
        );
    }

    writer->status = COMTRACE_ARCHIVE_SUCCESS;
    return COMTRACE_ARCHIVE_SUCCESS;
}

void comtrace_archive_writer_abort(COMTRACE_ARCHIVE_WRITER *writer)
{
    if (writer == NULL) {
        return;
    }

    if (writer_has_open_file(writer)) {
        (void)CloseHandle(writer->file);
        writer->file = INVALID_HANDLE_VALUE;
    }
}

void comtrace_archive_writer_destroy(COMTRACE_ARCHIVE_WRITER *writer)
{
    if (writer == NULL) {
        return;
    }

    comtrace_archive_writer_abort(writer);

    free(writer->final_path);
    free(writer->partial_path);

    ZeroMemory(writer, sizeof(*writer));
    writer->file = INVALID_HANDLE_VALUE;
}

const wchar_t *comtrace_archive_writer_partial_path(
    const COMTRACE_ARCHIVE_WRITER *writer
)
{
    if (writer == NULL) {
        return NULL;
    }

    return writer->partial_path;
}