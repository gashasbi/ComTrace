#include "event_record.h"

#include <stdio.h>
#include <string.h>

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

static int test_deep_copy(void)
{
    EVENT_RECORD source;
    EVENT_HEADER_EXTENDED_DATA_ITEM extended_data[2];
    BYTE user_data[] = {0x10U, 0x20U, 0x30U, 0x40U};
    BYTE related_data[] = {
        0x01U, 0x02U, 0x03U, 0x04U,
        0x05U, 0x06U, 0x07U, 0x08U,
        0x09U, 0x0AU, 0x0BU, 0x0CU,
        0x0DU, 0x0EU, 0x0FU, 0x10U
    };
    BYTE stack_data[] = {
        0xA1U, 0xA2U, 0xA3U, 0xA4U,
        0xA5U, 0xA6U, 0xA7U, 0xA8U
    };
    static const BYTE expected_user_data[] = {
        0x10U, 0x20U, 0x30U, 0x40U
    };
    static const BYTE expected_related_data[] = {
        0x01U, 0x02U, 0x03U, 0x04U,
        0x05U, 0x06U, 0x07U, 0x08U,
        0x09U, 0x0AU, 0x0BU, 0x0CU,
        0x0DU, 0x0EU, 0x0FU, 0x10U
    };
    static const BYTE expected_stack_data[] = {
        0xA1U, 0xA2U, 0xA3U, 0xA4U,
        0xA5U, 0xA6U, 0xA7U, 0xA8U
    };
    COMTRACE_OWNED_EVENT_RECORD *owned = NULL;
    COMTRACE_EVENT_RECORD_STATUS status;
    const BYTE *copied_related;
    const BYTE *copied_stack;
    int failures = 0;

    memset(&source, 0, sizeof(source));
    memset(extended_data, 0, sizeof(extended_data));

    source.EventHeader.ProviderId.Data1 = 0x12345678UL;
    source.EventHeader.EventDescriptor.Id = 77U;
    source.EventHeader.EventDescriptor.Version = 3U;
    source.EventHeader.EventDescriptor.Opcode = 11U;
    source.EventHeader.ProcessId = 1357UL;
    source.EventHeader.ThreadId = 2468UL;
    source.EventHeader.TimeStamp.QuadPart = 987654321LL;
    source.UserDataLength = (USHORT)sizeof(user_data);
    source.UserData = user_data;
    source.UserContext = (PVOID)(ULONG_PTR)1U;

    extended_data[0].Reserved1 = 4U;
    extended_data[0].ExtType = 1U;
    extended_data[0].DataSize = (USHORT)sizeof(related_data);
    extended_data[0].DataPtr = (ULONGLONG)(ULONG_PTR)related_data;

    extended_data[1].ExtType = 6U;
    extended_data[1].DataSize = (USHORT)sizeof(stack_data);
    extended_data[1].DataPtr = (ULONGLONG)(ULONG_PTR)stack_data;

    source.ExtendedDataCount = 2U;
    source.ExtendedData = extended_data;

    status = comtrace_event_record_copy(&source, &owned);

    check_condition(
        status == COMTRACE_EVENT_RECORD_SUCCESS,
        "Deep copy returns success",
        &failures
    );

    if (status != COMTRACE_EVENT_RECORD_SUCCESS || owned == NULL) {
        comtrace_event_record_destroy(owned);
        return failures + 1;
    }

    check_condition(
        owned->record.EventHeader.ProviderId.Data1 == 0x12345678UL,
        "Provider ID is copied",
        &failures
    );

    check_condition(
        owned->record.EventHeader.EventDescriptor.Id == 77U &&
        owned->record.EventHeader.EventDescriptor.Version == 3U &&
        owned->record.EventHeader.EventDescriptor.Opcode == 11U,
        "Event descriptor is copied",
        &failures
    );

    check_condition(
        owned->record.EventHeader.ProcessId == 1357UL &&
        owned->record.EventHeader.ThreadId == 2468UL &&
        owned->record.EventHeader.TimeStamp.QuadPart == 987654321LL,
        "Process, thread and timestamp are copied",
        &failures
    );

    check_condition(
        owned->record.UserData != source.UserData &&
        memcmp(owned->record.UserData, expected_user_data, sizeof(expected_user_data)) == 0,
        "User payload is deeply copied",
        &failures
    );

    check_condition(
        owned->record.ExtendedData != source.ExtendedData &&
        owned->record.ExtendedDataCount == 2U,
        "Extended data descriptors are copied",
        &failures
    );

    copied_related = (const BYTE *)(ULONG_PTR)
        owned->record.ExtendedData[0].DataPtr;
    copied_stack = (const BYTE *)(ULONG_PTR)
        owned->record.ExtendedData[1].DataPtr;

    check_condition(
        copied_related != related_data &&
        owned->record.ExtendedData[0].ExtType == 1U &&
        owned->record.ExtendedData[0].DataSize == sizeof(related_data) &&
        memcmp(copied_related, expected_related_data, sizeof(expected_related_data)) == 0,
        "First extended payload is deeply copied",
        &failures
    );

    check_condition(
        copied_stack != stack_data &&
        owned->record.ExtendedData[1].ExtType == 6U &&
        owned->record.ExtendedData[1].DataSize == sizeof(stack_data) &&
        memcmp(copied_stack, expected_stack_data, sizeof(expected_stack_data)) == 0,
        "Second extended payload is deeply copied",
        &failures
    );

    check_condition(
        owned->record.UserContext == NULL,
        "Transient callback context is not retained",
        &failures
    );

    user_data[0] = 0xFFU;
    related_data[0] = 0xFFU;
    stack_data[0] = 0xFFU;
    extended_data[0].ExtType = 99U;
    source.EventHeader.ProcessId = 9999UL;

    check_condition(
        ((const BYTE *)owned->record.UserData)[0] == 0x10U,
        "User payload remains valid after source changes",
        &failures
    );

    check_condition(
        copied_related[0] == 0x01U &&
        copied_stack[0] == 0xA1U,
        "Extended payloads remain valid after source changes",
        &failures
    );

    check_condition(
        owned->record.ExtendedData[0].ExtType == 1U &&
        owned->record.EventHeader.ProcessId == 1357UL,
        "Copied metadata remains independent",
        &failures
    );

    comtrace_event_record_destroy(owned);
    return failures;
}

static int test_empty_record(void)
{
    EVENT_RECORD source;
    COMTRACE_OWNED_EVENT_RECORD *owned = NULL;
    COMTRACE_EVENT_RECORD_STATUS status;
    int failures = 0;

    memset(&source, 0, sizeof(source));

    status = comtrace_event_record_copy(&source, &owned);

    check_condition(
        status == COMTRACE_EVENT_RECORD_SUCCESS && owned != NULL,
        "Empty event can be copied",
        &failures
    );

    if (owned != NULL) {
        check_condition(
            owned->record.UserData == NULL &&
            owned->record.ExtendedData == NULL &&
            owned->record.ExtendedDataCount == 0U,
            "Empty event has no borrowed data pointers",
            &failures
        );
    }

    comtrace_event_record_destroy(owned);
    comtrace_event_record_destroy(NULL);

    return failures;
}

static int test_invalid_sources(void)
{
    EVENT_RECORD source;
    EVENT_HEADER_EXTENDED_DATA_ITEM extended_data;
    COMTRACE_OWNED_EVENT_RECORD *owned = NULL;
    COMTRACE_EVENT_RECORD_STATUS status;
    int failures = 0;

    memset(&source, 0, sizeof(source));
    memset(&extended_data, 0, sizeof(extended_data));

    status = comtrace_event_record_copy(NULL, &owned);

    check_condition(
        status == COMTRACE_EVENT_RECORD_INVALID_ARGUMENT && owned == NULL,
        "Null source is rejected",
        &failures
    );

    status = comtrace_event_record_copy(&source, NULL);

    check_condition(
        status == COMTRACE_EVENT_RECORD_INVALID_ARGUMENT,
        "Null destination is rejected",
        &failures
    );

    source.UserDataLength = 1U;
    source.UserData = NULL;

    status = comtrace_event_record_copy(&source, &owned);

    check_condition(
        status == COMTRACE_EVENT_RECORD_INVALID_SOURCE && owned == NULL,
        "Missing user payload is rejected",
        &failures
    );

    memset(&source, 0, sizeof(source));
    source.ExtendedDataCount = 1U;
    source.ExtendedData = NULL;

    status = comtrace_event_record_copy(&source, &owned);

    check_condition(
        status == COMTRACE_EVENT_RECORD_INVALID_SOURCE && owned == NULL,
        "Missing extended data array is rejected",
        &failures
    );

    memset(&source, 0, sizeof(source));
    extended_data.DataSize = 1U;
    extended_data.DataPtr = 0ULL;
    source.ExtendedDataCount = 1U;
    source.ExtendedData = &extended_data;

    status = comtrace_event_record_copy(&source, &owned);

    check_condition(
        status == COMTRACE_EVENT_RECORD_INVALID_SOURCE && owned == NULL,
        "Missing extended payload is rejected",
        &failures
    );

    return failures;
}

int main(void)
{
    int failures = 0;

    failures += test_deep_copy();
    failures += test_empty_record();
    failures += test_invalid_sources();

    if (failures != 0) {
        fprintf(stderr, "%d event record test(s) failed\n", failures);
        return 1;
    }

    printf("Event record tests: PASS\n");
    return 0;
}