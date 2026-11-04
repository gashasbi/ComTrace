#include "archive_reader.h"
#include "archive_writer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define TEST_PATH_CAPACITY 512U

static void check_condition(int condition, const char *name, int *failures)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", name);
        ++(*failures);
    }
}

static int create_test_path(wchar_t *path, size_t capacity)
{
    wchar_t directory[MAX_PATH];
    wchar_t temporary[MAX_PATH];
    DWORD length;
    int written;

    length = GetTempPathW(MAX_PATH, directory);
    if (length == 0U || length >= MAX_PATH) {
        return 0;
    }

    if (GetTempFileNameW(directory, L"ctr", 0U, temporary) == 0U) {
        return 0;
    }

    if (!DeleteFileW(temporary)) {
        return 0;
    }

    written = swprintf_s(path, capacity, L"%ls.ctra", temporary);
    return written > 0;
}

static void delete_test_paths(const wchar_t *path)
{
    wchar_t partial[TEST_PATH_CAPACITY];

    if (path == NULL) {
        return;
    }

    (void)DeleteFileW(path);
    if (swprintf_s(partial, TEST_PATH_CAPACITY, L"%ls.partial", path) > 0) {
        (void)DeleteFileW(partial);
    }
}

static int create_archive(
    const wchar_t *path,
    const BYTE *user_data,
    USHORT user_data_length,
    const BYTE *extended_payload,
    USHORT extended_payload_length
)
{
    EVENT_RECORD source;
    EVENT_HEADER_EXTENDED_DATA_ITEM extended;
    COMTRACE_ARCHIVE_WRITER writer;
    COMTRACE_ARCHIVE_SUMMARY summary;
    COMTRACE_ARCHIVE_STATUS status;

    ZeroMemory(&source, sizeof(source));
    ZeroMemory(&extended, sizeof(extended));
    ZeroMemory(&writer, sizeof(writer));
    ZeroMemory(&summary, sizeof(summary));

    source.EventHeader.Size = 462U;
    source.EventHeader.ThreadId = 42U;
    source.EventHeader.ProcessId = 24U;
    source.EventHeader.TimeStamp.QuadPart = 123456789LL;
    source.EventHeader.ProviderId.Data1 = 0x11223344UL;
    source.EventHeader.EventDescriptor.Id = 91U;
    source.EventHeader.EventDescriptor.Opcode = 3U;
    source.EventHeader.ActivityId.Data1 = 0xAABBCCDDUL;
    source.BufferContext.ProcessorNumber = 7U;
    source.BufferContext.LoggerId = 0x1234U;
    source.UserData = (PVOID)user_data;
    source.UserDataLength = user_data_length;

    if (extended_payload_length != 0U) {
        extended.Reserved1 = 5U;
        extended.ExtType = 6U;
        extended.DataSize = extended_payload_length;
        extended.DataPtr = (ULONGLONG)(ULONG_PTR)extended_payload;
        source.ExtendedData = &extended;
        source.ExtendedDataCount = 1U;
    }

    status = comtrace_archive_writer_open(&writer, path);
    if (status == COMTRACE_ARCHIVE_SUCCESS) {
        status = comtrace_archive_writer_write_event(&writer, &source);
    }

    summary.process_trace_status = ERROR_SUCCESS;
    summary.buffers_read = 3U;
    summary.buffers_written = 3U;
    summary.trace_clock_type = 1U;
    summary.pointer_size = 8U;

    if (status == COMTRACE_ARCHIVE_SUCCESS) {
        status = comtrace_archive_writer_finalize(&writer, &summary);
    }

    comtrace_archive_writer_destroy(&writer);
    return status == COMTRACE_ARCHIVE_SUCCESS;
}

static int write_file_bytes(const wchar_t *path, const BYTE *data, DWORD size)
{
    HANDLE file;
    DWORD written = 0U;
    BOOL result;

    file = CreateFileW(
        path,
        GENERIC_WRITE,
        0U,
        NULL,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (file == INVALID_HANDLE_VALUE) {
        return 0;
    }

    result = WriteFile(file, data, size, &written, NULL);
    CloseHandle(file);
    return result && written == size;
}

static BYTE *read_file_bytes(const wchar_t *path, DWORD *size)
{
    HANDLE file;
    LARGE_INTEGER length;
    BYTE *data;
    DWORD received = 0U;

    *size = 0U;
    file = CreateFileW(
        path,
        GENERIC_READ,
        FILE_SHARE_READ,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    if (file == INVALID_HANDLE_VALUE) {
        return NULL;
    }

    if (!GetFileSizeEx(file, &length) || length.QuadPart < 0 ||
        (ULONGLONG)length.QuadPart > MAXDWORD) {
        CloseHandle(file);
        return NULL;
    }

    data = (BYTE *)malloc(length.QuadPart == 0 ? 1U : (size_t)length.QuadPart);
    if (data == NULL) {
        CloseHandle(file);
        return NULL;
    }

    if (length.QuadPart != 0 &&
        (!ReadFile(file, data, (DWORD)length.QuadPart, &received, NULL) ||
         received != (DWORD)length.QuadPart)) {
        free(data);
        CloseHandle(file);
        return NULL;
    }

    *size = received;
    CloseHandle(file);
    return data;
}

static int test_event_and_footer_round_trip(void)
{
    static const BYTE user_data[] = {0x10U, 0x20U, 0x30U, 0x40U};
    static const BYTE extended_payload[] = {0xAAU, 0xBBU, 0xCCU};
    COMTRACE_ARCHIVE_READER reader;
    COMTRACE_ARCHIVE_EVENT event;
    COMTRACE_ARCHIVE_READER_STATUS status;
    wchar_t path[TEST_PATH_CAPACITY];
    int failures = 0;

    if (!create_test_path(path, TEST_PATH_CAPACITY) ||
        !create_archive(
            path,
            user_data,
            (USHORT)sizeof(user_data),
            extended_payload,
            (USHORT)sizeof(extended_payload))) {
        fprintf(stderr, "Unable to create reader round-trip archive.\n");
        return 1;
    }

    ZeroMemory(&reader, sizeof(reader));
    ZeroMemory(&event, sizeof(event));
    status = comtrace_archive_reader_open(&reader, path);
    check_condition(status == COMTRACE_ARCHIVE_READER_SUCCESS,
        "Reader opens a complete archive", &failures);

    if (status == COMTRACE_ARCHIVE_READER_SUCCESS) {
        status = comtrace_archive_reader_next(&reader, &event);
        check_condition(status == COMTRACE_ARCHIVE_READER_SUCCESS,
            "Reader returns the first event", &failures);

        check_condition(
            event.record.EventHeader.Size == 462U &&
            event.record.EventHeader.ThreadId == 42U &&
            event.record.EventHeader.ProcessId == 24U &&
            event.record.EventHeader.TimeStamp.QuadPart == 123456789LL &&
            event.record.EventHeader.ProviderId.Data1 == 0x11223344UL &&
            event.record.EventHeader.EventDescriptor.Id == 91U &&
            event.record.EventHeader.ActivityId.Data1 == 0xAABBCCDDUL,
            "Event header fields are reconstructed", &failures);

        check_condition(
            event.record.BufferContext.ProcessorNumber == 7U &&
            event.record.BufferContext.LoggerId == 0x1234U,
            "Buffer context is reconstructed", &failures);

        check_condition(
            event.record.ExtendedDataCount == 1U &&
            event.record.ExtendedData[0].Reserved1 == 5U &&
            event.record.ExtendedData[0].ExtType == 6U &&
            event.record.ExtendedData[0].DataSize == sizeof(extended_payload) &&
            memcmp(
                (const void *)(ULONG_PTR)event.record.ExtendedData[0].DataPtr,
                extended_payload,
                sizeof(extended_payload)) == 0,
            "Extended data descriptor and bytes are reconstructed", &failures);

        check_condition(
            event.record.UserDataLength == sizeof(user_data) &&
            memcmp(event.record.UserData, user_data, sizeof(user_data)) == 0,
            "User payload is reconstructed", &failures);

        comtrace_archive_event_destroy(&event);
        status = comtrace_archive_reader_next(&reader, &event);
        check_condition(status == COMTRACE_ARCHIVE_READER_END,
            "Reader validates the footer and reaches end", &failures);

        check_condition(
            reader.footer.records_written == 1ULL &&
            reader.footer.buffers_read == 3U &&
            reader.footer.buffers_written == 3U &&
            reader.footer.completion_marker == 1U,
            "Footer metadata is reconstructed", &failures);

        check_condition(
            comtrace_archive_reader_next(&reader, &event) ==
                COMTRACE_ARCHIVE_READER_END,
            "End of archive remains stable", &failures);
    }

    comtrace_archive_event_destroy(&event);
    comtrace_archive_reader_close(&reader);
    delete_test_paths(path);
    return failures;
}

static int test_empty_archive(void)
{
    COMTRACE_ARCHIVE_READER reader;
    COMTRACE_ARCHIVE_EVENT event;
    COMTRACE_ARCHIVE_WRITER writer;
    COMTRACE_ARCHIVE_SUMMARY summary;
    wchar_t path[TEST_PATH_CAPACITY];
    int failures = 0;

    if (!create_test_path(path, TEST_PATH_CAPACITY)) {
        return 1;
    }

    ZeroMemory(&writer, sizeof(writer));
    ZeroMemory(&summary, sizeof(summary));
    summary.process_trace_status = ERROR_SUCCESS;

    if (comtrace_archive_writer_open(&writer, path) != COMTRACE_ARCHIVE_SUCCESS ||
        comtrace_archive_writer_finalize(&writer, &summary) != COMTRACE_ARCHIVE_SUCCESS) {
        comtrace_archive_writer_destroy(&writer);
        delete_test_paths(path);
        return 1;
    }

    comtrace_archive_writer_destroy(&writer);
    ZeroMemory(&reader, sizeof(reader));
    ZeroMemory(&event, sizeof(event));

    check_condition(
        comtrace_archive_reader_open(&reader, path) == COMTRACE_ARCHIVE_READER_SUCCESS,
        "Empty archive opens", &failures);
    check_condition(
        comtrace_archive_reader_next(&reader, &event) == COMTRACE_ARCHIVE_READER_END,
        "Empty archive has a valid footer", &failures);

    comtrace_archive_reader_close(&reader);
    delete_test_paths(path);
    return failures;
}

static int test_short_archive(void)
{
    static const BYTE invalid_header[24] = {0};
    COMTRACE_ARCHIVE_READER reader;
    wchar_t path[TEST_PATH_CAPACITY];
    int failures = 0;

    if (!create_test_path(path, TEST_PATH_CAPACITY) ||
        !write_file_bytes(path, invalid_header, sizeof(invalid_header))) {
        return 1;
    }

    ZeroMemory(&reader, sizeof(reader));
    check_condition(
        comtrace_archive_reader_open(&reader, path) ==
            COMTRACE_ARCHIVE_READER_TRUNCATED,
        "Short archive is rejected as truncated", &failures);

    comtrace_archive_reader_close(&reader);
    delete_test_paths(path);
    return failures;
}

static int test_invalid_header_signature(void)
{
    static const BYTE user_data[] = {0x01U};
    COMTRACE_ARCHIVE_READER reader;
    BYTE *data;
    DWORD size;
    wchar_t path[TEST_PATH_CAPACITY];
    int failures = 0;

    if (!create_test_path(path, TEST_PATH_CAPACITY) ||
        !create_archive(path, user_data, sizeof(user_data), NULL, 0U)) {
        return 1;
    }

    data = read_file_bytes(path, &size);
    if (data == NULL || size < 1U) {
        free(data);
        delete_test_paths(path);
        return 1;
    }

    data[0] ^= 0xFFU;
    if (!write_file_bytes(path, data, size)) {
        free(data);
        delete_test_paths(path);
        return 1;
    }
    free(data);

    ZeroMemory(&reader, sizeof(reader));
    check_condition(
        comtrace_archive_reader_open(&reader, path) ==
            COMTRACE_ARCHIVE_READER_INVALID_FORMAT,
        "Invalid archive signature is rejected", &failures);

    comtrace_archive_reader_close(&reader);
    delete_test_paths(path);
    return failures;
}

static int test_truncated_footer(void)
{
    static const BYTE user_data[] = {0x01U};
    COMTRACE_ARCHIVE_READER reader;
    COMTRACE_ARCHIVE_EVENT event;
    BYTE *data;
    DWORD size;
    wchar_t path[TEST_PATH_CAPACITY];
    int failures = 0;

    if (!create_test_path(path, TEST_PATH_CAPACITY) ||
        !create_archive(path, user_data, sizeof(user_data), NULL, 0U)) {
        return 1;
    }

    data = read_file_bytes(path, &size);
    if (data == NULL || size < 1U || !write_file_bytes(path, data, size - 1U)) {
        free(data);
        delete_test_paths(path);
        return 1;
    }
    free(data);

    ZeroMemory(&reader, sizeof(reader));
    ZeroMemory(&event, sizeof(event));
    check_condition(
        comtrace_archive_reader_open(&reader, path) == COMTRACE_ARCHIVE_READER_SUCCESS,
        "Archive with truncated footer opens its header", &failures);

    if (reader.file != INVALID_HANDLE_VALUE) {
        check_condition(
            comtrace_archive_reader_next(&reader, &event) ==
                COMTRACE_ARCHIVE_READER_TRUNCATED,
            "Truncated footer prevents a complete archive read", &failures);
    }

    comtrace_archive_event_destroy(&event);
    comtrace_archive_reader_close(&reader);
    delete_test_paths(path);
    return failures;
}

static int test_truncated_event_record(void)
{
    static const BYTE user_data[] = {0x11U, 0x22U, 0x33U};
    COMTRACE_ARCHIVE_READER reader;
    COMTRACE_ARCHIVE_EVENT event;
    BYTE *data;
    DWORD size;
    size_t footer_offset;
    wchar_t path[TEST_PATH_CAPACITY];
    int failures = 0;

    if (!create_test_path(path, TEST_PATH_CAPACITY) ||
        !create_archive(path, user_data, sizeof(user_data), NULL, 0U)) {
        return 1;
    }

    data = read_file_bytes(path, &size);
    if (data == NULL || size < 149U) {
        free(data);
        delete_test_paths(path);
        return 1;
    }

    footer_offset = (size_t)size - 124U;
    memmove(data + footer_offset - 1U, data + footer_offset, 124U);
    if (!write_file_bytes(path, data, size - 1U)) {
        free(data);
        delete_test_paths(path);
        return 1;
    }
    free(data);

    ZeroMemory(&reader, sizeof(reader));
    ZeroMemory(&event, sizeof(event));
    check_condition(
        comtrace_archive_reader_open(&reader, path) == COMTRACE_ARCHIVE_READER_SUCCESS,
        "Archive with truncated event opens its header", &failures);
    check_condition(
        comtrace_archive_reader_next(&reader, &event) ==
            COMTRACE_ARCHIVE_READER_TRUNCATED,
        "Truncated event payload is detected", &failures);

    comtrace_archive_event_destroy(&event);
    comtrace_archive_reader_close(&reader);
    delete_test_paths(path);
    return failures;
}

static int test_footer_record_count_mismatch(void)
{
    static const BYTE user_data[] = {0x11U};
    COMTRACE_ARCHIVE_READER reader;
    COMTRACE_ARCHIVE_EVENT event;
    BYTE *data;
    DWORD size;
    size_t footer_offset;
    wchar_t path[TEST_PATH_CAPACITY];
    int failures = 0;

    if (!create_test_path(path, TEST_PATH_CAPACITY) ||
        !create_archive(path, user_data, sizeof(user_data), NULL, 0U)) {
        return 1;
    }

    data = read_file_bytes(path, &size);
    if (data == NULL || size < 124U) {
        free(data);
        delete_test_paths(path);
        return 1;
    }

    footer_offset = (size_t)size - 124U;
    data[footer_offset + 16U] = 2U;
    if (!write_file_bytes(path, data, size)) {
        free(data);
        delete_test_paths(path);
        return 1;
    }
    free(data);

    ZeroMemory(&reader, sizeof(reader));
    ZeroMemory(&event, sizeof(event));
    check_condition(
        comtrace_archive_reader_open(&reader, path) == COMTRACE_ARCHIVE_READER_SUCCESS,
        "Archive with mismatched footer opens its header", &failures);

    if (reader.file != INVALID_HANDLE_VALUE) {
        check_condition(
            comtrace_archive_reader_next(&reader, &event) ==
                COMTRACE_ARCHIVE_READER_SUCCESS,
            "Event before mismatched footer is readable", &failures);
        comtrace_archive_event_destroy(&event);
        check_condition(
            comtrace_archive_reader_next(&reader, &event) ==
                COMTRACE_ARCHIVE_READER_INVALID_FORMAT,
            "Footer record count mismatch is rejected", &failures);
    }

    comtrace_archive_event_destroy(&event);
    comtrace_archive_reader_close(&reader);
    delete_test_paths(path);
    return failures;
}

static int test_invalid_record_type(void)
{
    static const BYTE user_data[] = {0x01U};
    COMTRACE_ARCHIVE_READER reader;
    COMTRACE_ARCHIVE_EVENT event;
    BYTE *data;
    DWORD size;
    wchar_t path[TEST_PATH_CAPACITY];
    int failures = 0;

    if (!create_test_path(path, TEST_PATH_CAPACITY) ||
        !create_archive(path, user_data, sizeof(user_data), NULL, 0U)) {
        return 1;
    }

    data = read_file_bytes(path, &size);
    if (data == NULL || size < 25U) {
        free(data);
        delete_test_paths(path);
        return 1;
    }

    data[24U] = 2U;
    if (!write_file_bytes(path, data, size)) {
        free(data);
        delete_test_paths(path);
        return 1;
    }
    free(data);

    ZeroMemory(&reader, sizeof(reader));
    ZeroMemory(&event, sizeof(event));
    check_condition(
        comtrace_archive_reader_open(&reader, path) == COMTRACE_ARCHIVE_READER_SUCCESS,
        "Archive with invalid record type opens header", &failures);
    check_condition(
        comtrace_archive_reader_next(&reader, &event) ==
            COMTRACE_ARCHIVE_READER_INVALID_FORMAT,
        "Invalid record type is rejected", &failures);

    comtrace_archive_reader_close(&reader);
    delete_test_paths(path);
    return failures;
}

int main(void)
{
    int failures = 0;

    failures += test_event_and_footer_round_trip();
    failures += test_empty_archive();
    failures += test_short_archive();
    failures += test_invalid_header_signature();
    failures += test_truncated_footer();
    failures += test_truncated_event_record();
    failures += test_footer_record_count_mismatch();
    failures += test_invalid_record_type();

    if (failures != 0) {
        fprintf(stderr, "%d archive reader test(s) failed\n", failures);
        return 1;
    }

    printf("Archive reader tests: PASS\n");
    return 0;
}
