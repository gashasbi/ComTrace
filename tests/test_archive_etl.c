#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <evntrace.h>
#include <evntprov.h>
#include <evntcons.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "etl_archive.h"
#include "archive_reader.h"

static const GUID test_provider_id = {
    0x7b1d8a31, 0x4f2c, 0x4e61,
    { 0x9a, 0x21, 0x63, 0x74, 0x85, 0x96, 0xa7, 0xb8 }
};

static const GUID test_session_id = {
    0x2c8e7b41, 0x5d3f, 0x4a72,
    { 0x8b, 0x32, 0x74, 0x85, 0x96, 0xa7, 0xb8, 0xc9 }
};

static const BYTE payload_one[] = "ComTrace ETW integration event one";
static const BYTE payload_two[] = "ComTrace ETW integration event two";

static int create_etl(const wchar_t *etl_path)
{
    ULONG status;
    ULONG stop_status;
    TRACEHANDLE session = 0;
    REGHANDLE provider = 0;
    EVENT_TRACE_PROPERTIES *properties = NULL;
    wchar_t logger_name[96];
    size_t logger_chars;
    size_t path_chars;
    size_t properties_size;
    ULONG logger_offset;
    ULONG path_offset;
    EVENT_DESCRIPTOR descriptor;
    EVENT_DATA_DESCRIPTOR data;
    int result = 0;

    if (_snwprintf_s(
            logger_name,
            sizeof(logger_name) / sizeof(logger_name[0]),
            _TRUNCATE,
            L"ComTraceArchiveEtlTest_%lu",
            (unsigned long)GetCurrentProcessId()) < 0) {
        return 0;
    }

    logger_chars = wcslen(logger_name) + 1U;
    path_chars = wcslen(etl_path) + 1U;
    logger_offset = (ULONG)sizeof(EVENT_TRACE_PROPERTIES);
    path_offset = logger_offset + (ULONG)(logger_chars * sizeof(wchar_t));
    properties_size = (size_t)path_offset + path_chars * sizeof(wchar_t);

    properties = (EVENT_TRACE_PROPERTIES *)calloc(1U, properties_size);
    if (properties == NULL) {
        return 0;
    }

    properties->Wnode.BufferSize = (ULONG)properties_size;
    properties->Wnode.Guid = test_session_id;
    properties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    properties->Wnode.ClientContext = 1U;
    properties->BufferSize = 64U;
    properties->MinimumBuffers = 2U;
    properties->MaximumBuffers = 8U;
    properties->LogFileMode = EVENT_TRACE_FILE_MODE_SEQUENTIAL;
    properties->MaximumFileSize = 5U;
    properties->LoggerNameOffset = logger_offset;
    properties->LogFileNameOffset = path_offset;

    memcpy((BYTE *)properties + logger_offset, logger_name,
           logger_chars * sizeof(wchar_t));
    memcpy((BYTE *)properties + path_offset, etl_path,
           path_chars * sizeof(wchar_t));

    status = StartTraceW(&session, logger_name, properties);
    if (status != ERROR_SUCCESS) {
        goto cleanup;
    }

    status = EventRegister(&test_provider_id, NULL, NULL, &provider);
    if (status != ERROR_SUCCESS) {
        goto stop_session;
    }

    status = EnableTraceEx2(
        session,
        &test_provider_id,
        EVENT_CONTROL_CODE_ENABLE_PROVIDER,
        TRACE_LEVEL_VERBOSE,
        0ULL,
        0U,
          5000U,
        NULL);
    if (status != ERROR_SUCCESS) {
        goto stop_session;
    }

    ZeroMemory(&descriptor, sizeof(descriptor));
    descriptor.Id = 1U;
    descriptor.Level = TRACE_LEVEL_INFORMATION;
    descriptor.Keyword = 1ULL;
    EventDataDescCreate(&data, (PVOID)payload_one, (ULONG)(sizeof(payload_one) - 1U));
    status = EventWrite(provider, &descriptor, 1U, &data);
    if (status != ERROR_SUCCESS) {
        goto stop_session;
    }

    descriptor.Id = 2U;
    EventDataDescCreate(&data, (PVOID)payload_two, (ULONG)(sizeof(payload_two) - 1U));
    status = EventWrite(provider, &descriptor, 1U, &data);
    if (status != ERROR_SUCCESS) {
        goto stop_session;
    }

    result = 1;

stop_session:
    (void)EventUnregister(provider);
    provider = 0;
    stop_status = ControlTraceW(
        session,
        logger_name,
        properties,
        EVENT_TRACE_CONTROL_STOP);
    if (stop_status != ERROR_SUCCESS) {
        result = 0;
    }
    session = 0;

cleanup:
    if (provider != 0) {
        (void)EventUnregister(provider);
    }
    if (session != 0) {
        (void)ControlTraceW(
            session,
            logger_name,
            properties,
            EVENT_TRACE_CONTROL_STOP);
    }
    free(properties);
    return result;
}

static int verify_archive(const wchar_t *archive_path)
{
    static const BYTE *expected_payloads[] = {
        payload_one,
        payload_two
    };
    static const USHORT expected_lengths[] = {
        (USHORT)(sizeof(payload_one) - 1U),
        (USHORT)(sizeof(payload_two) - 1U)
    };
    COMTRACE_ARCHIVE_READER reader;
    COMTRACE_ARCHIVE_EVENT event;
    COMTRACE_ARCHIVE_READER_STATUS status;
    ULONG index = 0U;
    ULONGLONG records_seen = 0ULL;
    int result = 0;

    ZeroMemory(&reader, sizeof(reader));
    ZeroMemory(&event, sizeof(event));

    status = comtrace_archive_reader_open(&reader, archive_path);
    if (status != COMTRACE_ARCHIVE_READER_SUCCESS) {
        return 0;
    }

    for (;;) {
        status = comtrace_archive_reader_next(&reader, &event);
        if (status == COMTRACE_ARCHIVE_READER_END) {
            break;
        }
        if (status != COMTRACE_ARCHIVE_READER_SUCCESS) {
            goto cleanup;
        }
        ++records_seen;
        if (memcmp(&event.record.EventHeader.ProviderId,
                   &test_provider_id,
                   sizeof(test_provider_id)) != 0) {
            comtrace_archive_event_destroy(&event);
            continue;
        }

        if (index >= 2U ||
            event.record.EventHeader.EventDescriptor.Id != (USHORT)(index + 1U) ||
            event.record.UserDataLength != expected_lengths[index] ||
            event.record.UserData == NULL ||
            memcmp(event.record.UserData, expected_payloads[index],
                   expected_lengths[index]) != 0) {
            comtrace_archive_event_destroy(&event);
            goto cleanup;
        }

        ++index;
        comtrace_archive_event_destroy(&event);
    }

    if (index == 2U &&
        reader.footer.records_written == records_seen &&
        reader.footer.process_trace_status == ERROR_SUCCESS &&
        reader.footer.completion_marker == 1U) {
        result = 1;
    }

cleanup:
    comtrace_archive_event_destroy(&event);
    comtrace_archive_reader_close(&reader);
    return result;
}

int wmain(void)
{
    wchar_t temp_path[MAX_PATH];
    wchar_t etl_path[MAX_PATH];
    wchar_t archive_path[MAX_PATH];
    DWORD path_length;
    int result = EXIT_FAILURE;

    path_length = GetTempPathW(MAX_PATH, temp_path);
    if (path_length == 0U || path_length >= MAX_PATH) {
        return EXIT_FAILURE;
    }

    if (_snwprintf_s(
            etl_path, MAX_PATH, _TRUNCATE,
            L"%lsComTraceArchiveEtl_%lu.etl",
            temp_path, (unsigned long)GetCurrentProcessId()) < 0 ||
        _snwprintf_s(
            archive_path, MAX_PATH, _TRUNCATE,
            L"%lsComTraceArchiveEtl_%lu.ctra",
            temp_path, (unsigned long)GetCurrentProcessId()) < 0) {
        return EXIT_FAILURE;
    }

    (void)DeleteFileW(etl_path);
    (void)DeleteFileW(archive_path);

    if (!create_etl(etl_path)) {
        fwprintf(stderr, L"Unable to create ETW test trace. Error: %lu\n",
                 (unsigned long)GetLastError());
        goto cleanup;
    }

    if (comtrace_archive_etl(etl_path, archive_path) != EXIT_SUCCESS) {
        fwprintf(stderr, L"ETL to CTRA conversion failed.\n");
        goto cleanup;
    }

    if (!verify_archive(archive_path)) {
        fwprintf(stderr, L"CTRA readback validation failed.\n");
        goto cleanup;
    }

    result = EXIT_SUCCESS;

cleanup:
    (void)DeleteFileW(etl_path);
    (void)DeleteFileW(archive_path);
    return result;
}
