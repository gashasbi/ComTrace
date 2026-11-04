#include "archive_writer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#define TEST_PATH_CAPACITY 512U

static void check_condition(
    int condition,
    const char *name,
    int *failures
)
{
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", name);
        ++(*failures);
    }
}

static int create_test_path(
    wchar_t *path,
    size_t path_capacity
)
{
    wchar_t directory[MAX_PATH];
    wchar_t temporary[MAX_PATH];
    DWORD length;
    int written;

    length = GetTempPathW(MAX_PATH, directory);

    if (length == 0U || length >= MAX_PATH) {
        return 0;
    }

    if (GetTempFileNameW(directory, L"cta", 0U, temporary) == 0U) {
        return 0;
    }

    if (!DeleteFileW(temporary)) {
        return 0;
    }

    written = swprintf_s(
        path,
        path_capacity,
        L"%ls.ctra",
        temporary
    );

    return written > 0;
}

static void delete_test_paths(const wchar_t *path)
{
    wchar_t partial[TEST_PATH_CAPACITY];
    int written;

    if (path == NULL) {
        return;
    }

    (void)DeleteFileW(path);

    written = swprintf_s(
        partial,
        TEST_PATH_CAPACITY,
        L"%ls.partial",
        path
    );

    if (written > 0) {
        (void)DeleteFileW(partial);
    }
}

static BYTE *read_archive(
    const wchar_t *path,
    size_t *size
)
{
    FILE *file = NULL;
    long file_length;
    BYTE *data;

    if (path == NULL || size == NULL) {
        return NULL;
    }

    *size = 0U;

    if (_wfopen_s(&file, path, L"rb") != 0 || file == NULL) {
        return NULL;
    }

    if (fseek(file, 0L, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }

    file_length = ftell(file);

    if (file_length < 0L ||
        fseek(file, 0L, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    data = (BYTE *)malloc(
        file_length == 0L ? 1U : (size_t)file_length
    );

    if (data == NULL) {
        fclose(file);
        return NULL;
    }

    if (fread(data, 1U, (size_t)file_length, file) !=
        (size_t)file_length) {
        free(data);
        fclose(file);
        return NULL;
    }

    fclose(file);
    *size = (size_t)file_length;

    return data;
}

static USHORT read_u16(const BYTE *data)
{
    return (USHORT)(
        (USHORT)data[0] |
        ((USHORT)data[1] << 8U)
    );
}

static ULONG read_u32(const BYTE *data)
{
    return
        (ULONG)data[0] |
        ((ULONG)data[1] << 8U) |
        ((ULONG)data[2] << 16U) |
        ((ULONG)data[3] << 24U);
}

static ULONGLONG read_u64(const BYTE *data)
{
    ULONGLONG value = 0ULL;
    unsigned int index;

    for (index = 0U; index < 8U; ++index) {
        value |= (ULONGLONG)data[index] << (index * 8U);
    }

    return value;
}

static int test_archive_round_trip(void)
{
    static const BYTE user_data[] = {
        0xA0U, 0xA1U, 0xA2U, 0xA3U, 0xA4U
    };
    static const BYTE first_extended[] = {
        0x11U, 0x22U, 0x33U, 0x44U
    };
    static const BYTE second_extended[] = {
        0xAAU, 0xBBU, 0xCCU
    };
    static const BYTE first_descriptor[] = {
        0x01U, 0x00U, 0x02U, 0x00U,
        0x01U, 0x00U, 0x04U, 0x00U
    };
    static const BYTE second_descriptor[] = {
        0x03U, 0x00U, 0x04U, 0x00U,
        0x00U, 0x80U, 0x03U, 0x00U
    };
    static const BYTE expected_magic[8] = {
        'C', 'T', 'R', 'A', 'C', 'E', 'A', '1'
    };
    static const BYTE expected_footer[8] = {
        'C', 'T', 'R', 'F', 'T', 'R', '0', '1'
    };
    EVENT_RECORD source;
    EVENT_HEADER_EXTENDED_DATA_ITEM extended[2];
    COMTRACE_ARCHIVE_WRITER writer;
    COMTRACE_ARCHIVE_SUMMARY summary;
    COMTRACE_ARCHIVE_STATUS status;
    GUID expected_provider;
    GUID expected_activity;
    BYTE first_linkage[2] = {0x01U, 0x00U};
    BYTE second_linkage[2] = {0x00U, 0x80U};
    BYTE *archive = NULL;
    size_t archive_size = 0U;
    const size_t event_offset = 48U;
    const size_t footer_offset = 164U;
    wchar_t path[TEST_PATH_CAPACITY];
    int failures = 0;

    memset(&source, 0, sizeof(source));
    memset(extended, 0, sizeof(extended));
    memset(&writer, 0, sizeof(writer));
    memset(&summary, 0, sizeof(summary));
    memset(&expected_provider, 0, sizeof(expected_provider));
    memset(&expected_activity, 0, sizeof(expected_activity));

    if (!create_test_path(path, TEST_PATH_CAPACITY)) {
        fprintf(stderr, "Unable to create test path.\n");
        return 1;
    }

    source.EventHeader.Size = 80U;
    source.EventHeader.HeaderType = 2U;
    source.EventHeader.Flags = 0x1234U;
    source.EventHeader.EventProperty = 3U;
    source.EventHeader.ThreadId = 2468UL;
    source.EventHeader.ProcessId = 1357UL;
    source.EventHeader.TimeStamp.QuadPart = 987654321LL;

    source.EventHeader.ProviderId.Data1 = 0x11223344UL;
    source.EventHeader.ProviderId.Data2 = 0x5566U;
    source.EventHeader.ProviderId.Data3 = 0x7788U;

    source.EventHeader.EventDescriptor.Id = 77U;
    source.EventHeader.EventDescriptor.Version = 3U;
    source.EventHeader.EventDescriptor.Channel = 4U;
    source.EventHeader.EventDescriptor.Level = 5U;
    source.EventHeader.EventDescriptor.Opcode = 6U;
    source.EventHeader.EventDescriptor.Task = 7U;
    source.EventHeader.EventDescriptor.Keyword = 0x1122334455667788ULL;

    source.EventHeader.KernelTime = 17U;
    source.EventHeader.UserTime = 19U;
    source.EventHeader.ActivityId.Data1 = 0xAABBCCDDUL;
    source.EventHeader.ActivityId.Data2 = 0x1122U;
    source.EventHeader.ActivityId.Data3 = 0x3344U;

    expected_provider = source.EventHeader.ProviderId;
    expected_activity = source.EventHeader.ActivityId;

    source.BufferContext.ProcessorNumber = 9U;
    source.BufferContext.Alignment = 10U;
    source.BufferContext.LoggerId = 0x1234U;

    extended[0].Reserved1 = 1U;
    extended[0].ExtType = 2U;
    extended[0].DataSize = (USHORT)sizeof(first_extended);
    extended[0].DataPtr = (ULONGLONG)(ULONG_PTR)first_extended;
    memcpy((BYTE *)&extended[0] + 4U, first_linkage, sizeof(first_linkage));

    extended[1].Reserved1 = 3U;
    extended[1].ExtType = 4U;
    extended[1].DataSize = (USHORT)sizeof(second_extended);
    extended[1].DataPtr = (ULONGLONG)(ULONG_PTR)second_extended;
    memcpy((BYTE *)&extended[1] + 4U, second_linkage, sizeof(second_linkage));

    source.ExtendedDataCount = 2U;
    source.ExtendedData = extended;
    source.UserDataLength = (USHORT)sizeof(user_data);
    source.UserData = (PVOID)user_data;
    source.UserContext = (PVOID)(ULONG_PTR)1U;

    status = comtrace_archive_writer_open(&writer, path);

    check_condition(
        status == COMTRACE_ARCHIVE_SUCCESS,
        "Archive writer opens",
        &failures
    );

    if (status != COMTRACE_ARCHIVE_SUCCESS) {
        comtrace_archive_writer_destroy(&writer);
        delete_test_paths(path);
        return failures + 1;
    }

    status = comtrace_archive_writer_write_event(&writer, &source);

    check_condition(
        status == COMTRACE_ARCHIVE_SUCCESS &&
        writer.records_written == 1ULL,
        "Event is written",
        &failures
    );

    summary.events_lost = 7U;
    summary.buffers_lost = 8U;
    summary.buffers_written = 9U;
    summary.buffers_read = 10U;
    summary.trace_clock_type = 11U;
    summary.pointer_size = 8U;
    summary.os_version = 0xAABBCCDDUL;
    summary.provider_version = 22631U;
    summary.number_of_processors = 12U;
    summary.buffer_size = 65536U;
    summary.timer_resolution = 156250U;
    summary.maximum_file_size = 64U;
    summary.log_file_mode = 0x100U;
    summary.start_buffers = 2U;
    summary.cpu_speed_mhz = 3200U;
    summary.start_time = 0x0102030405060708ULL;
    summary.end_time = 0x1112131415161718ULL;
    summary.boot_time = 0x2122232425262728ULL;
    summary.perf_frequency = 10000000ULL;
    summary.process_trace_status = ERROR_SUCCESS;

    status = comtrace_archive_writer_finalize(&writer, &summary);

    check_condition(
        status == COMTRACE_ARCHIVE_SUCCESS,
        "Archive is finalized",
        &failures
    );

    comtrace_archive_writer_destroy(&writer);

    archive = read_archive(path, &archive_size);

    check_condition(
        archive != NULL,
        "Final archive can be read",
        &failures
    );

    if (archive == NULL) {
        delete_test_paths(path);
        return failures + 1;
    }

    check_condition(
        archive_size == 288U,
        "Archive has expected complete size",
        &failures
    );

    if (archive_size >= 288U) {
        check_condition(
            memcmp(archive, expected_magic, sizeof(expected_magic)) == 0,
            "Archive signature is correct",
            &failures
        );

        check_condition(
            read_u32(archive + 8U) == 1U &&
            read_u32(archive + 12U) == 24U &&
            read_u32(archive + 16U) == 1U,
            "Archive header version and flags are correct",
            &failures
        );

        check_condition(
            read_u32(archive + 24U) == 1U &&
            read_u32(archive + 28U) == 0U &&
            read_u64(archive + 32U) == 1ULL &&
            read_u64(archive + 40U) == 116ULL,
            "Record frame is correct",
            &failures
        );

        check_condition(
            read_u16(archive + event_offset) == 80U &&
            read_u16(archive + event_offset + 2U) == 2U &&
            read_u16(archive + event_offset + 4U) == 0x1234U &&
            read_u16(archive + event_offset + 6U) == 3U,
            "Event header prefix is preserved",
            &failures
        );

        check_condition(
            read_u32(archive + event_offset + 8U) == 2468UL &&
            read_u32(archive + event_offset + 12U) == 1357UL &&
            read_u64(archive + event_offset + 16U) == 987654321ULL,
            "Thread, process and timestamp are preserved",
            &failures
        );

        check_condition(
            memcmp(
                archive + event_offset + 24U,
                &expected_provider,
                sizeof(expected_provider)
            ) == 0,
            "Provider GUID is preserved",
            &failures
        );

        check_condition(
            read_u16(archive + event_offset + 40U) == 77U &&
            archive[event_offset + 42U] == 3U &&
            archive[event_offset + 43U] == 4U &&
            archive[event_offset + 44U] == 5U &&
            archive[event_offset + 45U] == 6U &&
            read_u16(archive + event_offset + 46U) == 7U &&
            read_u64(archive + event_offset + 48U) ==
                0x1122334455667788ULL,
            "Event descriptor is preserved",
            &failures
        );

        check_condition(
            read_u32(archive + event_offset + 56U) == 17U &&
            read_u32(archive + event_offset + 60U) == 19U &&
            memcmp(
                archive + event_offset + 64U,
                &expected_activity,
                sizeof(expected_activity)
            ) == 0,
            "Event timing and activity fields are preserved",
            &failures
        );

        check_condition(
            archive[128U] == 9U &&
            archive[129U] == 10U &&
            read_u16(archive + 130U) == 0x1234U &&
            read_u16(archive + 132U) == 2U &&
            read_u16(archive + 134U) == sizeof(user_data),
            "Buffer context and event lengths are preserved",
            &failures
        );

        check_condition(
            memcmp(archive + 136U, first_descriptor, 8U) == 0 &&
            memcmp(archive + 144U, first_extended, sizeof(first_extended)) == 0,
            "First extended data descriptor and payload are preserved",
            &failures
        );

        check_condition(
            memcmp(archive + 148U, second_descriptor, 8U) == 0 &&
            memcmp(archive + 156U, second_extended, sizeof(second_extended)) == 0,
            "Second extended data descriptor and payload are preserved",
            &failures
        );

        check_condition(
            memcmp(
                archive + 159U,
                user_data,
                sizeof(user_data)
            ) == 0,
            "User payload is preserved",
            &failures
        );

        check_condition(
            memcmp(
                archive + footer_offset,
                expected_footer,
                sizeof(expected_footer)
            ) == 0,
            "Archive footer signature is correct",
            &failures
        );

        check_condition(
            read_u32(archive + footer_offset + 8U) == 1U &&
            read_u32(archive + footer_offset + 12U) == 124U &&
            read_u64(archive + footer_offset + 16U) == 1ULL,
            "Archive footer version and record count are correct",
            &failures
        );

        check_condition(
            read_u32(archive + footer_offset + 24U) == 7U &&
            read_u32(archive + footer_offset + 28U) == 8U &&
            read_u32(archive + footer_offset + 32U) == 9U &&
            read_u32(archive + footer_offset + 36U) == 10U &&
            read_u32(archive + footer_offset + 40U) == ERROR_SUCCESS &&
            read_u32(archive + footer_offset + 44U) == 1U,
            "Loss counters and completion marker are preserved",
            &failures
        );

        check_condition(
            read_u32(archive + footer_offset + 48U) == 11U &&
            read_u32(archive + footer_offset + 52U) == 8U &&
            read_u32(archive + footer_offset + 56U) == 0xAABBCCDDUL &&
            read_u32(archive + footer_offset + 60U) == 22631U &&
            read_u32(archive + footer_offset + 64U) == 12U &&
            read_u32(archive + footer_offset + 68U) == 65536U &&
            read_u32(archive + footer_offset + 72U) == 156250U &&
            read_u32(archive + footer_offset + 76U) == 64U &&
            read_u32(archive + footer_offset + 80U) == 0x100U &&
            read_u32(archive + footer_offset + 84U) == 2U &&
            read_u32(archive + footer_offset + 88U) == 3200U,
            "Trace clock and system metadata are preserved",
            &failures
        );

        check_condition(
            read_u64(archive + footer_offset + 92U) ==
                0x0102030405060708ULL &&
            read_u64(archive + footer_offset + 100U) ==
                0x1112131415161718ULL &&
            read_u64(archive + footer_offset + 108U) ==
                0x2122232425262728ULL &&
            read_u64(archive + footer_offset + 116U) == 10000000ULL,
            "Trace time base metadata is preserved",
            &failures
        );
    }

    free(archive);
    delete_test_paths(path);

    return failures;
}

static int test_existing_output_is_not_overwritten(void)
{
    COMTRACE_ARCHIVE_WRITER writer;
    COMTRACE_ARCHIVE_STATUS status;
    HANDLE file;
    BYTE marker = 0x5AU;
    DWORD written = 0U;
    BYTE actual = 0U;
    DWORD read_count = 0U;
    wchar_t path[TEST_PATH_CAPACITY];
    int failures = 0;

    if (!create_test_path(path, TEST_PATH_CAPACITY)) {
        fprintf(stderr, "Unable to create existing-output test path.\n");
        return 1;
    }

    file = CreateFileW(
        path,
        GENERIC_WRITE,
        0U,
        NULL,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    check_condition(
        file != INVALID_HANDLE_VALUE,
        "Existing-output test file is created",
        &failures
    );

    if (file == INVALID_HANDLE_VALUE) {
        delete_test_paths(path);
        return failures + 1;
    }

    if (!WriteFile(file, &marker, 1U, &written, NULL) ||
        written != 1U) {
        CloseHandle(file);
        delete_test_paths(path);
        return failures + 1;
    }

    CloseHandle(file);

    status = comtrace_archive_writer_open(&writer, path);

    check_condition(
        status == COMTRACE_ARCHIVE_OUTPUT_EXISTS,
        "Existing output is rejected",
        &failures
    );

    comtrace_archive_writer_destroy(&writer);

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
        check_condition(0, "Existing output remains accessible", &failures);
    } else {
        check_condition(
            ReadFile(file, &actual, 1U, &read_count, NULL) &&
            read_count == 1U &&
            actual == marker,
            "Existing output contents remain unchanged",
            &failures
        );

        CloseHandle(file);
    }

    delete_test_paths(path);
    return failures;
}

static int test_incomplete_archive_is_kept_as_partial(void)
{
    COMTRACE_ARCHIVE_WRITER writer;
    COMTRACE_ARCHIVE_STATUS status;
    wchar_t path[TEST_PATH_CAPACITY];
    wchar_t partial[TEST_PATH_CAPACITY];
    int written;
    int failures = 0;

    memset(&writer, 0, sizeof(writer));

    if (!create_test_path(path, TEST_PATH_CAPACITY)) {
        fprintf(stderr, "Unable to create partial archive test path.\n");
        return 1;
    }

    status = comtrace_archive_writer_open(&writer, path);

    check_condition(
        status == COMTRACE_ARCHIVE_SUCCESS,
        "Partial archive writer opens",
        &failures
    );

    if (status != COMTRACE_ARCHIVE_SUCCESS) {
        comtrace_archive_writer_destroy(&writer);
        delete_test_paths(path);
        return failures + 1;
    }

    written = swprintf_s(
        partial,
        TEST_PATH_CAPACITY,
        L"%ls.partial",
        path
    );

    check_condition(
        written > 0 &&
        GetFileAttributesW(comtrace_archive_writer_partial_path(&writer)) !=
            INVALID_FILE_ATTRIBUTES,
        "Partial archive exists before abort",
        &failures
    );

    comtrace_archive_writer_abort(&writer);

    check_condition(
        GetFileAttributesW(partial) != INVALID_FILE_ATTRIBUTES,
        "Incomplete archive remains available after abort",
        &failures
    );

    check_condition(
        GetFileAttributesW(path) == INVALID_FILE_ATTRIBUTES,
        "Incomplete archive is not published as final",
        &failures
    );

    comtrace_archive_writer_destroy(&writer);
    delete_test_paths(path);

    return failures;
}

static int test_invalid_event_is_rejected(void)
{
    EVENT_RECORD source;
    COMTRACE_ARCHIVE_WRITER writer;
    COMTRACE_ARCHIVE_STATUS status;
    wchar_t path[TEST_PATH_CAPACITY];
    int failures = 0;

    memset(&source, 0, sizeof(source));
    memset(&writer, 0, sizeof(writer));

    if (!create_test_path(path, TEST_PATH_CAPACITY)) {
        fprintf(stderr, "Unable to create invalid-event test path.\n");
        return 1;
    }

    status = comtrace_archive_writer_open(&writer, path);

    if (status != COMTRACE_ARCHIVE_SUCCESS) {
        comtrace_archive_writer_destroy(&writer);
        delete_test_paths(path);
        return 1;
    }

    source.UserDataLength = 1U;
    source.UserData = NULL;

    status = comtrace_archive_writer_write_event(&writer, &source);

    check_condition(
        status == COMTRACE_ARCHIVE_INVALID_EVENT,
        "Missing user payload is rejected",
        &failures
    );

    comtrace_archive_writer_abort(&writer);
    comtrace_archive_writer_destroy(&writer);
    delete_test_paths(path);

    return failures;
}

static int test_unopened_writer_rejects_operations(void)
{
    EVENT_RECORD event_record;
    COMTRACE_ARCHIVE_WRITER writer;
    COMTRACE_ARCHIVE_SUMMARY summary;
    COMTRACE_ARCHIVE_STATUS status;
    int failures = 0;

    memset(&event_record, 0, sizeof(event_record));
    memset(&writer, 0, sizeof(writer));
    memset(&summary, 0, sizeof(summary));

    status = comtrace_archive_writer_write_event(
        &writer,
        &event_record
    );

    check_condition(
        status == COMTRACE_ARCHIVE_INVALID_ARGUMENT,
        "Unopened writer rejects event writes",
        &failures
    );

    status = comtrace_archive_writer_finalize(&writer, &summary);

    check_condition(
        status == COMTRACE_ARCHIVE_INVALID_ARGUMENT,
        "Unopened writer rejects finalization",
        &failures
    );

    comtrace_archive_writer_abort(&writer);
    comtrace_archive_writer_destroy(&writer);

    return failures;
}
int main(void)
{
    int failures = 0;

    failures += test_archive_round_trip();
    failures += test_existing_output_is_not_overwritten();
    failures += test_incomplete_archive_is_kept_as_partial();
    failures += test_invalid_event_is_rejected();
    failures += test_unopened_writer_rejects_operations();

    if (failures != 0) {
        fprintf(stderr, "%d archive writer test(s) failed\n", failures);
        return 1;
    }

    printf("Archive writer tests: PASS\n");
    return 0;
}