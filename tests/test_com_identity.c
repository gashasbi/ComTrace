#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <evntrace.h>
#include <evntcons.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include "com_identity.h"
#include "etl_decoder.h"

static const GUID fixture_provider_id = {
    0xd3a24c71, 0x91b6, 0x4b02,
    { 0x8c, 0x37, 0x5a, 0x6d, 0x7e, 0x80, 0x91, 0xaf }
};

static const GUID expected_class_id = {
    0x00112233, 0x4455, 0x6677,
    { 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff }
};

static const GUID expected_interface_id = {
    0x10213243, 0x5465, 0x7687,
    { 0x98, 0xa9, 0xba, 0xcb, 0xdc, 0xed, 0xfe, 0x0f }
};

static const ULONG expected_target_process_id = 0x12345678UL;
static const ULONG expected_target_thread_id = 0x87654321UL;
static const ULONG expected_source_process_id = 0x76543210UL;
static const ULONG expected_source_thread_id = 0x10203040UL;

static const GUID expected_ipid = {
    0x40516273, 0x8495, 0xa6b7,
    { 0xc8, 0xd9, 0xea, 0xfb, 0x0c, 0x1d, 0x2e, 0x3f }
};

static const GUID expected_rpc_interface_uuid = {
    0x50617283, 0x94a5, 0xb6c7,
    { 0xd8, 0xe9, 0xfa, 0x0b, 0x1c, 0x2d, 0x3e, 0x4f }
};

static const GUID expected_rpcss_interface_uuid = {
    0x60718293, 0xa4b5, 0xc6d7,
    { 0xe8, 0xf9, 0x0a, 0x1b, 0x2c, 0x3d, 0x4e, 0x5f }
};

static const GUID expected_rpcss_object_uuid = {
    0x708192a3, 0xb4c5, 0xd6e7,
    { 0xf8, 0x09, 0x1a, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f }
};

static const GUID expected_type_manager_uuid = {
    0x8091a2b3, 0xc4d5, 0xe6f7,
    { 0x08, 0x19, 0x2a, 0x3b, 0x4c, 0x5d, 0x6e, 0x7f }
};

static const ULONGLONG expected_oid = 0x1122334455667788ULL;
static const ULONGLONG expected_hex_oid = 0xAABBCCDDEEFF0011ULL;
static const ULONGLONG expected_call_trace_id = 0x8877665544332211ULL;
static const ULONGLONG expected_call_trace_id_32 = 0x12345678ULL;

static const GUID expected_activity_id = {
    0x20314253, 0x6475, 0x8697,
    { 0xa8, 0xb9, 0xca, 0xdb, 0xec, 0xfd, 0x0e, 0x1f }
};

static const GUID expected_related_activity_id = {
    0x30415263, 0x7485, 0x96a7,
    { 0xb8, 0xc9, 0xda, 0xeb, 0xfc, 0x0d, 0x1e, 0x2f }
};

typedef struct {
    ULONG events_seen;
    ULONG failures;
} IDENTITY_TEST_CONTEXT;

static VOID WINAPI identity_event_callback(PEVENT_RECORD event_record)
{
    IDENTITY_TEST_CONTEXT *context;
    GUID value;
    ULONG number = 0U;
    ULONGLONG wide_number = 0U;
    LONG signed_number = 0;
    wchar_t string_value[64];
    COMTRACE_ACTIVITY_IDS activity_ids;
    COMTRACE_COM_OBJECT_IDENTITY object_identity;
    COMTRACE_ETL_PROPERTY_VALUE decoded_value;
    EVENT_PROPERTY_INFO decoder_property;
    const wchar_t *parent_names[1];
    ULONG parent_indices[1];
    COMTRACE_ETL_PROPERTY_READ_STATUS decoder_status;
    COMTRACE_IDENTITY_STATUS status;

    if (event_record == NULL || event_record->UserContext == NULL) {
        return;
    }

    context = (IDENTITY_TEST_CONTEXT *)event_record->UserContext;

    if (memcmp(
            &event_record->EventHeader.ProviderId,
            &fixture_provider_id,
            sizeof(fixture_provider_id)
        ) != 0) {
        return;
    }

    ++context->events_seen;

    if (event_record->EventHeader.EventDescriptor.Id == 3U) {
        status = comtrace_extract_call_trace_id(event_record, &wide_number);
        if (status != COMTRACE_IDENTITY_SUCCESS ||
            wide_number != expected_call_trace_id_32) {
            fwprintf(stderr, L"32-bit CallTraceId extraction failed: status=%u value=%llu\\n", (unsigned int)status, (unsigned long long)wide_number);
            ++context->failures;
        }

        status = comtrace_extract_uint32_property(
            event_record,
            L"CallTraceId",
            &number
        );
        if (status != COMTRACE_IDENTITY_SUCCESS ||
            (ULONGLONG)number != expected_call_trace_id_32) {
            fwprintf(stderr, L"32-bit CallTraceId typed extraction failed: status=%u value=%lu\\n", (unsigned int)status, (unsigned long)number);
            ++context->failures;
        }

        status = comtrace_extract_uint64_property(
            event_record,
            L"CallTraceId",
            &wide_number
        );
        if (status != COMTRACE_IDENTITY_PROPERTY_UNSUPPORTED) {
            fwprintf(stderr, L"32-bit CallTraceId type rejection failed: status=%u\\n", (unsigned int)status);
            ++context->failures;
        }

        status = comtrace_extract_com_object_identity(
            event_record,
            &object_identity
        );
        if (status != COMTRACE_IDENTITY_SUCCESS ||
            !object_identity.has_call_trace_id ||
            object_identity.call_trace_id != expected_call_trace_id_32) {
            fwprintf(stderr, L"32-bit CallTraceId identity model failed: status=%u\\n", (unsigned int)status);
            ++context->failures;
        }

        return;
    }

    if (event_record->EventHeader.EventDescriptor.Id == 6U) {
        static const struct {
            const wchar_t *name;
            const GUID *expected;
        } properties[] = {
            {L"InterfaceUuid", &expected_rpc_interface_uuid},
            {L"InterfaceUUID", &expected_rpcss_interface_uuid},
            {L"ObjectUUID", &expected_rpcss_object_uuid},
            {L"TypeMgrUuid", &expected_type_manager_uuid}
        };
        size_t index;

        for (index = 0U; index < sizeof(properties) / sizeof(properties[0]); ++index) {
            status = comtrace_extract_guid_property(
                event_record,
                properties[index].name,
                &value
            );
            if (status != COMTRACE_IDENTITY_SUCCESS ||
                memcmp(&value, properties[index].expected, sizeof(GUID)) != 0) {
                fwprintf(stderr, L"%ls GUID extraction failed: status=%u\n",
                    properties[index].name,
                    (unsigned int)status);
                ++context->failures;
            }
        }
        return;
    }

    if (event_record->EventHeader.EventDescriptor.Id == 5U) {
        status = comtrace_extract_com_object_identity(
            event_record,
            &object_identity
        );
        if (status != COMTRACE_IDENTITY_SUCCESS ||
            !object_identity.has_oid ||
            !object_identity.has_iid ||
            !object_identity.has_ipid ||
            object_identity.has_clsid ||
            object_identity.has_call_trace_id ||
            object_identity.oid != expected_oid ||
            memcmp(&object_identity.iid, &expected_interface_id, sizeof(GUID)) != 0 ||
            memcmp(&object_identity.ipid, &expected_ipid, sizeof(GUID)) != 0) {
            fwprintf(stderr, L"COM rundown identity property extraction failed: status=%u\n",
                (unsigned int)status);
            ++context->failures;
        }
        return;
    }

    if (event_record->EventHeader.EventDescriptor.Id == 4U) {
        status = comtrace_extract_activity_ids(event_record, &activity_ids);
        if (status != COMTRACE_IDENTITY_SUCCESS ||
            activity_ids.has_activity_id ||
            !activity_ids.has_related_activity_id ||
            memcmp(
                &activity_ids.related_activity_id,
                &expected_related_activity_id,
                sizeof(GUID)
            ) != 0) {
            fwprintf(stderr, L"Related-only ActivityId extraction failed: status=%u primary=%u related=%u\n",
                (unsigned int)status,
                (unsigned int)activity_ids.has_activity_id,
                (unsigned int)activity_ids.has_related_activity_id);
            ++context->failures;
        }
        return;
    }

    if (event_record->EventHeader.EventDescriptor.Id == 2U) {
        status = comtrace_extract_guid_property(event_record, L"ClassId", &value);
        if (status != COMTRACE_IDENTITY_PROPERTY_NOT_FOUND) {
            fwprintf(stderr, L"ClassId alias fixture unexpectedly exposed ClassId: status=%u\n", (unsigned int)status);
            ++context->failures;
        }

        status = comtrace_extract_guid_property(event_record, L"CLSID", &value);
        if (status != COMTRACE_IDENTITY_SUCCESS ||
            memcmp(&value, &expected_class_id, sizeof(GUID)) != 0) {
            fwprintf(stderr, L"CLSID alias extraction failed: status=%u\n", (unsigned int)status);
            ++context->failures;
        }

        status = comtrace_extract_com_object_identity(event_record, &object_identity);
        if (status != COMTRACE_IDENTITY_SUCCESS ||
            !object_identity.has_clsid ||
            object_identity.has_iid ||
            object_identity.has_ipid ||
            object_identity.has_oid ||
            object_identity.has_call_trace_id ||
            memcmp(&object_identity.clsid, &expected_class_id, sizeof(GUID)) != 0) {
            fwprintf(stderr, L"CLSID alias identity model failed: status=%u\n", (unsigned int)status);
            ++context->failures;
        }
        return;
    }

    if (event_record->EventHeader.EventDescriptor.Id != 1U) {
        fwprintf(stderr, L"Unexpected identity fixture event ID: %u\\n", (unsigned int)event_record->EventHeader.EventDescriptor.Id);
        ++context->failures;
        return;
    }

    ZeroMemory(&decoder_property, sizeof(decoder_property));
    ZeroMemory(&decoded_value, sizeof(decoded_value));
    decoder_status = comtrace_etl_read_property_value(
        event_record,
        &decoder_property,
        L"ClassId",
        NULL,
        NULL,
        0U,
        0xFFFFFFFFUL,
        &decoded_value
    );
    if (decoder_status != COMTRACE_ETL_PROPERTY_READ_SUCCESS ||
        decoded_value.size != sizeof(expected_class_id) ||
        memcmp(decoded_value.data, &expected_class_id, sizeof(expected_class_id)) != 0) {
        fwprintf(stderr, L"Raw ETW decoder validation failed: status=%u\n", (unsigned int)decoder_status);
        ++context->failures;
    }
    comtrace_etl_property_value_destroy(&decoded_value);

    decoder_property.Flags = PropertyStruct;
    decoder_status = comtrace_etl_read_property_value(
        event_record,
        &decoder_property,
        L"ClassId",
        NULL,
        NULL,
        0U,
        0xFFFFFFFFUL,
        &decoded_value
    );
    if (decoder_status != COMTRACE_ETL_PROPERTY_READ_UNSUPPORTED) {
        fwprintf(stderr, L"Raw ETW decoder complex property rejection failed: status=%u\n", (unsigned int)decoder_status);
        ++context->failures;
    }

    decoder_property.Flags = 0U;
    decoder_status = comtrace_etl_read_property_value(
        event_record,
        &decoder_property,
        L"ClassId",
        NULL,
        NULL,
        9U,
        0xFFFFFFFFUL,
        &decoded_value
    );
    if (decoder_status != COMTRACE_ETL_PROPERTY_READ_INVALID_ARGUMENT) {
        fwprintf(stderr, L"Raw ETW decoder path depth validation failed: status=%u\n", (unsigned int)decoder_status);
        ++context->failures;
    }

    parent_names[0] = L"Parent";
    parent_indices[0] = 0xFFFFFFFFUL;
    decoder_status = comtrace_etl_read_property_value(
        event_record,
        &decoder_property,
        L"ClassId",
        parent_names,
        NULL,
        1U,
        0xFFFFFFFFUL,
        &decoded_value
    );
    if (decoder_status != COMTRACE_ETL_PROPERTY_READ_INVALID_ARGUMENT) {
        fwprintf(stderr, L"Raw ETW decoder missing path index validation failed: status=%u\n", (unsigned int)decoder_status);
        ++context->failures;
    }

    parent_names[0] = NULL;
    decoder_status = comtrace_etl_read_property_value(
        event_record,
        &decoder_property,
        L"ClassId",
        parent_names,
        parent_indices,
        1U,
        0xFFFFFFFFUL,
        &decoded_value
    );
    if (decoder_status != COMTRACE_ETL_PROPERTY_READ_INVALID_ARGUMENT) {
        fwprintf(stderr, L"Raw ETW decoder null path name validation failed: status=%u\n", (unsigned int)decoder_status);
        ++context->failures;
    }

    status = comtrace_extract_guid_property(event_record, L"ClassId", &value);
    if (status != COMTRACE_IDENTITY_SUCCESS ||
        memcmp(&value, &expected_class_id, sizeof(value)) != 0) {
        fwprintf(stderr, L"ClassId extraction failed: status=%u\n", (unsigned int)status);
        ++context->failures;
    }

    status = comtrace_extract_guid_property(event_record, L"TargetInterface", &value);
    if (status != COMTRACE_IDENTITY_SUCCESS ||
        memcmp(&value, &expected_interface_id, sizeof(value)) != 0) {
        fwprintf(stderr, L"TargetInterface extraction failed: status=%u\n", (unsigned int)status);
        ++context->failures;
    }

    status = comtrace_extract_uint32_property(
        event_record,
        L"TargetProcessId",
        &number
    );
    if (status != COMTRACE_IDENTITY_SUCCESS ||
        number != expected_target_process_id) {
        fwprintf(stderr, L"TargetProcessId extraction failed: status=%u value=%lu\n", (unsigned int)status, (unsigned long)number);
        ++context->failures;
    }

    status = comtrace_extract_uint32_property(
        event_record,
        L"TargetThreadId",
        &number
    );
    if (status != COMTRACE_IDENTITY_SUCCESS ||
        number != expected_target_thread_id) {
        fwprintf(stderr, L"TargetThreadId extraction failed: status=%u value=%lu\n", (unsigned int)status, (unsigned long)number);
        ++context->failures;
    }

    status = comtrace_extract_uint32_property(
        event_record,
        L"HexTargetProcessId",
        &number
    );
    if (status != COMTRACE_IDENTITY_SUCCESS || number != expected_target_process_id) {
        fwprintf(stderr, L"HexTargetProcessId extraction failed: status=%u value=%lu\n", (unsigned int)status, (unsigned long)number);
        ++context->failures;
    }

    status = comtrace_extract_uint32_property(event_record, L"NotGuid", &number);
    if (status != COMTRACE_IDENTITY_SUCCESS || number != 123U) {
        fwprintf(stderr, L"NotGuid UINT32 extraction failed: status=%u value=%lu\n", (unsigned int)status, (unsigned long)number);
        ++context->failures;
    }

    status = comtrace_extract_guid_property(event_record, L"NotGuid", &value);
    if (status != COMTRACE_IDENTITY_PROPERTY_UNSUPPORTED) {
        fwprintf(stderr, L"NotGuid GUID type rejection failed: status=%u\n", (unsigned int)status);
        ++context->failures;
    }

    status = comtrace_extract_string_property(
        event_record,
        L"DisplayName",
        string_value,
        _countof(string_value)
    );
    if (status != COMTRACE_IDENTITY_SUCCESS ||
        wcscmp(string_value, L"ComTrace Object") != 0) {
        fwprintf(stderr, L"Unicode identity string extraction failed: status=%u\n", (unsigned int)status);
        ++context->failures;
    }

    status = comtrace_extract_string_property(
        event_record,
        L"AnsiName",
        string_value,
        _countof(string_value)
    );
    if (status != COMTRACE_IDENTITY_SUCCESS ||
        wcscmp(string_value, L"ComTrace ANSI") != 0) {
        fwprintf(stderr, L"ANSI identity string extraction failed: status=%u\n", (unsigned int)status);
        ++context->failures;
    }

    string_value[0] = L'X';
    status = comtrace_extract_string_property(
        event_record,
        L"MissingName",
        string_value,
        _countof(string_value)
    );
    if (status != COMTRACE_IDENTITY_PROPERTY_NOT_FOUND ||
        string_value[0] != L'\0') {
        fwprintf(stderr, L"Missing identity string handling failed: status=%u\n", (unsigned int)status);
        ++context->failures;
    }

    status = comtrace_extract_string_property(
        event_record,
        L"NotGuid",
        string_value,
        _countof(string_value)
    );
    if (status != COMTRACE_IDENTITY_PROPERTY_UNSUPPORTED) {
        fwprintf(stderr, L"Non-string identity property rejection failed: status=%u\n", (unsigned int)status);
        ++context->failures;
    }

    status = comtrace_extract_string_property(
        event_record,
        L"DisplayName",
        string_value,
        2U
    );
    if (status != COMTRACE_IDENTITY_VALUE_UNAVAILABLE) {
        fwprintf(stderr, L"Short identity string buffer handling failed: status=%u\n", (unsigned int)status);
        ++context->failures;
    }

    value = expected_class_id;
    status = comtrace_extract_guid_property(event_record, L"MissingGuid", &value);
    if (status != COMTRACE_IDENTITY_PROPERTY_NOT_FOUND ||
        memcmp(&value, &expected_class_id, sizeof(GUID)) != 0) {
        fwprintf(stderr, L"MissingGuid lookup changed its output: status=%u\n", (unsigned int)status);
        ++context->failures;
    }

    status = comtrace_extract_activity_ids(event_record, &activity_ids);
    if (status != COMTRACE_IDENTITY_SUCCESS ||
        !activity_ids.has_activity_id ||
        !activity_ids.has_related_activity_id ||
        memcmp(&activity_ids.activity_id, &expected_activity_id, sizeof(GUID)) != 0 ||
        memcmp(&activity_ids.related_activity_id, &expected_related_activity_id, sizeof(GUID)) != 0) {
        fwprintf(stderr, L"Activity ID extraction failed: status=%u\n", (unsigned int)status);
        ++context->failures;
    }

    status = comtrace_extract_guid_property(event_record, L"IPID", &value);
    if (status != COMTRACE_IDENTITY_SUCCESS ||
        memcmp(&value, &expected_ipid, sizeof(GUID)) != 0) {
        fwprintf(stderr, L"IPID extraction failed: status=%u\n", (unsigned int)status);
        ++context->failures;
    }

    status = comtrace_extract_int32_property(event_record, L"HRESULT", &signed_number);
    if (status != COMTRACE_IDENTITY_SUCCESS ||
        (ULONG)signed_number != 0x80004005UL) {
        fwprintf(stderr, L"HRESULT extraction failed: status=%u value=0x%08lx\n", (unsigned int)status, (unsigned long)signed_number);
        ++context->failures;
    }

    status = comtrace_extract_int32_property(event_record, L"HResultHex", &signed_number);
    if (status != COMTRACE_IDENTITY_SUCCESS ||
        (ULONG)signed_number != 0x80070005UL) {
        fwprintf(stderr, L"HResultHex extraction failed: status=%u value=0x%08lx\n", (unsigned int)status, (unsigned long)signed_number);
        ++context->failures;
    }

    status = comtrace_extract_uint64_property(event_record, L"OID", &wide_number);
    if (status != COMTRACE_IDENTITY_SUCCESS || wide_number != expected_oid) {
        fwprintf(stderr, L"OID extraction failed: status=%u value=%llu\n", (unsigned int)status, (unsigned long long)wide_number);
        ++context->failures;
    }

    status = comtrace_extract_uint64_property(event_record, L"HexOID", &wide_number);
    if (status != COMTRACE_IDENTITY_SUCCESS || wide_number != expected_hex_oid) {
        fwprintf(stderr, L"HexOID extraction failed: status=%u value=%llu\n", (unsigned int)status, (unsigned long long)wide_number);
        ++context->failures;
    }

    status = comtrace_extract_uint64_property(
        event_record,
        L"CallTraceId",
        &wide_number
    );
    if (status != COMTRACE_IDENTITY_SUCCESS ||
        wide_number != expected_call_trace_id) {
        fwprintf(stderr, L"CallTraceId extraction failed: status=%u value=%llu\n", (unsigned int)status, (unsigned long long)wide_number);
        ++context->failures;
    }

    status = comtrace_extract_uint64_property(event_record, L"NotGuid", &wide_number);
    if (status != COMTRACE_IDENTITY_PROPERTY_UNSUPPORTED) {
        fwprintf(stderr, L"NotGuid UINT64 rejection failed: status=%u\n", (unsigned int)status);
        ++context->failures;
    }

    status = comtrace_extract_com_object_identity(event_record, &object_identity);
    if (status != COMTRACE_IDENTITY_SUCCESS ||
        !object_identity.has_clsid ||
        !object_identity.has_iid ||
        !object_identity.has_ipid ||
        !object_identity.has_oid ||
        !object_identity.has_call_trace_id ||
        !object_identity.has_process_id ||
        !object_identity.has_thread_id ||
        !object_identity.has_source_process_id ||
        !object_identity.has_source_thread_id ||
        !object_identity.has_timestamp ||
        object_identity.timestamp != event_record->EventHeader.TimeStamp.QuadPart ||
        object_identity.target_process_id != expected_target_process_id ||
        object_identity.target_thread_id != expected_target_thread_id ||
        object_identity.source_process_id != expected_source_process_id ||
        object_identity.source_thread_id != expected_source_thread_id ||
        memcmp(&object_identity.clsid, &expected_class_id, sizeof(GUID)) != 0 ||
        memcmp(&object_identity.iid, &expected_interface_id, sizeof(GUID)) != 0 ||
        memcmp(&object_identity.ipid, &expected_ipid, sizeof(GUID)) != 0 ||
        object_identity.oid != expected_oid ||
        object_identity.call_trace_id != expected_call_trace_id) {
        fwprintf(stderr, L"COM object identity model validation failed: status=%u\n", (unsigned int)status);
        ++context->failures;
    }
}

static int verify_event(const wchar_t *etl_path)
{
    EVENT_TRACE_LOGFILEW logfile;
    TRACEHANDLE trace_handle;
    ULONG process_status;
    ULONG close_status;
    IDENTITY_TEST_CONTEXT context;

    ZeroMemory(&logfile, sizeof(logfile));
    ZeroMemory(&context, sizeof(context));

    logfile.LogFileName = (LPWSTR)etl_path;
    logfile.ProcessTraceMode = PROCESS_TRACE_MODE_EVENT_RECORD;
    logfile.EventRecordCallback = identity_event_callback;
    logfile.Context = &context;

    trace_handle = OpenTraceW(&logfile);
    if (trace_handle == INVALID_PROCESSTRACE_HANDLE) {
        fwprintf(stderr, L"OpenTraceW failed: %lu\n", (unsigned long)GetLastError());
        return 0;
    }

    process_status = ProcessTrace(&trace_handle, 1U, NULL, NULL);
    close_status = CloseTrace(trace_handle);

    if (process_status != ERROR_SUCCESS || close_status != ERROR_SUCCESS) {
        fwprintf(
            stderr,
            L"Trace processing failed. ProcessTrace: %lu CloseTrace: %lu\n",
            (unsigned long)process_status,
            (unsigned long)close_status
        );
        return 0;
    }

    if (context.events_seen != 6U || context.failures != 0U) {
        fwprintf(
            stderr,
            L"Identity validation failed. Events: %lu Failures: %lu\n",
            (unsigned long)context.events_seen,
            (unsigned long)context.failures
        );
        return 0;
    }

    return 1;
}

int wmain(int argc, wchar_t **argv)
{
    GUID value;

    if (argc != 2) {
        fwprintf(stderr, L"Usage: ComTraceComIdentityTests <etl-path>\n");
        return EXIT_FAILURE;
    }

    if (comtrace_extract_guid_property(NULL, L"ClassId", &value) !=
            COMTRACE_IDENTITY_INVALID_ARGUMENT ||
        comtrace_extract_guid_property(NULL, L"ClassId", NULL) !=
            COMTRACE_IDENTITY_INVALID_ARGUMENT) {
        fwprintf(stderr, L"Invalid argument handling failed.\n");
        return EXIT_FAILURE;
    }

    {
        wchar_t process_path[1024];
        if (!comtrace_get_process_image_path(
                GetCurrentProcessId(),
                process_path,
                (DWORD)_countof(process_path)
            ) ||
            process_path[0] == L'\0') {
            fwprintf(stderr, L"Current process image path lookup failed.\n");
            return EXIT_FAILURE;
        }
        if (comtrace_get_process_image_path(0U, process_path, (DWORD)_countof(process_path)) ||
            process_path[0] != L'\0') {
            fwprintf(stderr, L"Invalid process image path lookup was accepted.\n");
            return EXIT_FAILURE;
        }
    }

    if (!verify_event(argv[1])) {
        return EXIT_FAILURE;
    }

    wprintf(L"COM identity properties and process image path tests passed.\n");
    return EXIT_SUCCESS;
}
