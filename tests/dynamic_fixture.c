#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <evntrace.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dynamic_fixture.h"

static const GUID session_id = {
    0x3d9f6c52, 0x6e40, 0x4b83,
    { 0x8b, 0x32, 0x74, 0x85, 0x96, 0xa7, 0xb8, 0xc9 }
};

int wmain(int argc, wchar_t **argv)
{
    BYTE storage[sizeof(EVENT_TRACE_PROPERTIES) + 256U + MAX_PATH * sizeof(wchar_t)];
    EVENT_TRACE_PROPERTIES *properties = (EVENT_TRACE_PROPERTIES *)storage;
    TRACEHANDLE session = 0;
    wchar_t logger_name[96];
    size_t logger_size;
    size_t path_size;
    ULONG path_offset;
    ULONG status;
    ULONG stop_status;
    unsigned short count = 3U;
    int values[3] = { 11, 22, 33 };
    unsigned short length = 4U;
    unsigned char data[4] = { 0xDE, 0xAD, 0xBE, 0xEF };
    ULONG hex_count = 2U;
    int hex_values[2] = { 44, 55 };
    BYTE byte_count = 2U;
    SHORT byte_values[2] = { -7, 8 };
    unsigned short text_length = 6U;
    wchar_t text[] = L"Hello";
    unsigned short utf8_text_length = 6U;
    char utf8_text[] = "caf\xc3\xa9";
    unsigned short json_text_length = 20U;
    char json_text[] = "{\"message\":\"caf\xc3\xa9\"}";
    unsigned short xml_text_length = 13U;
    char xml_text[] = "<v>caf\xc3\xa9</v>";
    unsigned char fixed_data[4] = { 0x11, 0x22, 0x33, 0x44 };
    wchar_t fixed_text[] = L"Fixed";
    unsigned char large_data[5000];
    SID_IDENTIFIER_AUTHORITY sid_authority = SECURITY_NT_AUTHORITY;
    PSID test_sid = NULL;
    int registered = 0;
    unsigned long long wide_value = 9007199254740993ULL;
    ULONG ipv4_address = 0x0100007FUL;
    BYTE ipv6_address[16] = {
        0x20, 0x01, 0x0D, 0xB8, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01
    };
    BYTE socket_address[16] = {
        0x02, 0x00, 0x01, 0xBB, 0xC0, 0x00, 0x02, 0x0A,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    BYTE socket_address_v6[28] = {
        0x17, 0x00, 0x01, 0xBB, 0x00, 0x00, 0x00, 0x00,
        0x20, 0x01, 0x0D, 0xB8, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01,
        0x00, 0x00, 0x00, 0x00
    };
    BYTE socket_address_other[16] = {
        0x34, 0x12, 0x01, 0x02, 0x11, 0x22, 0x33, 0x44,
        0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC
    };
    LONG hresult_value = (LONG)0x80004005UL;
    BYTE hex8_value = 0xABU;
    USHORT hex16_value = 0xCDEFU;
    ULONG hex32_value = 0x89ABCDEFUL;
    ULONGLONG hex64_value = 0x0123456789ABCDEFULL;
    ULONG error_code_value = 0x00000005UL;
    ULONG ntstatus_value = 0xC0000005UL;
    ULONG code_pointer_value = 0x12345678UL;
    int result = 1;

    if (argc != 2) {
        fwprintf(stderr, L"Usage: ComTraceDynamicFixture <etl-path>\n");
        return 1;
    }

    if (_snwprintf_s(
            logger_name,
            sizeof(logger_name) / sizeof(logger_name[0]),
            _TRUNCATE,
            L"ComTraceDynamicFixture_%lu",
            (unsigned long)GetCurrentProcessId()) < 0) {
        return 1;
    }

    logger_size = (wcslen(logger_name) + 1U) * sizeof(wchar_t);
    path_size = (wcslen(argv[1]) + 1U) * sizeof(wchar_t);
    path_offset = (ULONG)sizeof(EVENT_TRACE_PROPERTIES) + (ULONG)logger_size;

    if (path_offset + path_size > sizeof(storage)) {
        return 1;
    }

    ZeroMemory(storage, sizeof(storage));
    properties->Wnode.BufferSize = (ULONG)(path_offset + path_size);
    properties->Wnode.Guid = session_id;
    properties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    properties->Wnode.ClientContext = 1U;
    properties->BufferSize = 64U;
    properties->MinimumBuffers = 2U;
    properties->MaximumBuffers = 8U;
    properties->LogFileMode = EVENT_TRACE_FILE_MODE_SEQUENTIAL;
    properties->MaximumFileSize = 5U;
    properties->LoggerNameOffset = (ULONG)sizeof(EVENT_TRACE_PROPERTIES);
    properties->LogFileNameOffset = path_offset;

    CopyMemory(storage + properties->LoggerNameOffset, logger_name, logger_size);
    CopyMemory(storage + path_offset, argv[1], path_size);

    status = StartTraceW(&session, logger_name, properties);
    if (status != ERROR_SUCCESS) {
        fwprintf(stderr, L"StartTraceW failed: %lu\n", (unsigned long)status);
        goto cleanup;
    }

    status = EventRegisterComTraceDynamicFixture();
    if (status != ERROR_SUCCESS) {
        fwprintf(stderr, L"EventRegister failed: %lu\n", (unsigned long)status);
        goto cleanup;
    }
    registered = 1;

    status = EnableTraceEx2(
        session,
        &COMTRACE_DYNAMIC_PROVIDER,
        EVENT_CONTROL_CODE_ENABLE_PROVIDER,
        TRACE_LEVEL_VERBOSE,
        0ULL,
        0U,
        5000U,
        NULL);
    if (status != ERROR_SUCCESS) {
        fwprintf(stderr, L"EnableTraceEx2 failed: %lu\n", (unsigned long)status);
        goto cleanup;
    }

    if (!AllocateAndInitializeSid(&sid_authority, 2, SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &test_sid)) {
        fwprintf(stderr, L"AllocateAndInitializeSid failed: %lu\n", (unsigned long)GetLastError());
        goto cleanup;
    }

    memset(large_data, 0xAB, sizeof(large_data));

    status = EventWriteDynamicEvent(count, values, length, data, text_length, text, utf8_text_length, utf8_text, json_text_length, json_text, xml_text_length, xml_text, fixed_data, fixed_text, large_data, hex_count, hex_values, byte_count, byte_values, test_sid, wide_value, ipv4_address, ipv6_address, socket_address, socket_address_v6, socket_address_other, hresult_value, hex8_value, hex16_value, hex32_value, hex64_value, error_code_value, ntstatus_value, code_pointer_value);
    if (status != ERROR_SUCCESS) {
        fwprintf(stderr, L"EventWriteDynamicEvent failed: %lu\n", (unsigned long)status);
        goto cleanup;
    }

    result = 0;

cleanup:
    if (test_sid != NULL) {
        FreeSid(test_sid);
    }
    if (registered) {
        EventUnregisterComTraceDynamicFixture();
    }

    if (session != 0) {
        stop_status = ControlTraceW(
            session,
            logger_name,
            properties,
            EVENT_TRACE_CONTROL_STOP);
        if (stop_status != ERROR_SUCCESS) {
            fwprintf(stderr, L"ControlTraceW failed: %lu\n", (unsigned long)stop_status);
            result = 1;
        }
    }

    return result;
}
