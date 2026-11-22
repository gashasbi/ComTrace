#include "etl_property_format.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

static int expect_name(const wchar_t *actual, const wchar_t *expected)
{
    if (wcscmp(actual, expected) != 0) {
        fwprintf(stderr, L"Expected %ls, got %ls\n", expected, actual);
        return 0;
    }
    return 1;
}

static int expect_normalized_payload(
    USHORT input_type,
    BYTE *buffer,
    ULONG buffer_size,
    ULONG pointer_size,
    USHORT expected_type,
    ULONG expected_offset,
    const BYTE *expected_data,
    ULONG expected_size
)
{
    BYTE *payload = buffer;
    ULONG payload_size = buffer_size;

    if (!comtrace_etl_normalize_property_payload(
            &input_type,
            &payload,
            &payload_size,
            pointer_size) ||
        input_type != expected_type ||
        payload != buffer + expected_offset ||
        payload_size != expected_size ||
        (expected_size != 0U &&
         memcmp(payload, expected_data, expected_size) != 0)) {
        fwprintf(stderr, L"ETW payload normalization failed for type %u\n", (unsigned int)input_type);
        return 0;
    }

    return 1;
}

static int test_payload_normalization(void)
{
    BYTE counted_unicode[] = { 4U, 0U, 'T', 0U, 'e', 0U };
    BYTE reversed_ansi[] = { 0U, 4U, 'T', 'e', 's', 't' };
    BYTE counted_binary[] = { 2U, 0U, 0xAAU, 0xBBU };
    BYTE hex_dump[] = { 4U, 0U, 0U, 0U, 0x11U, 0x22U, 0x33U, 0x44U };
    BYTE nonterminated[] = { 'O', 0U, 'K', 0U };
    BYTE wbem_sid[28] = {
        0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
        0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U,
        1U, 1U, 0U, 0U, 0U, 0U, 0U, 5U,
        21U, 0U, 0U, 0U
    };
    static const BYTE expected_unicode[] = { 'T', 0U, 'e', 0U };
    static const BYTE expected_ansi[] = { 'T', 'e', 's', 't' };
    static const BYTE expected_binary[] = { 0xAAU, 0xBBU };
    static const BYTE expected_hex[] = { 0x11U, 0x22U, 0x33U, 0x44U };
    static const BYTE expected_sid[] = {
        1U, 1U, 0U, 0U, 0U, 0U, 0U, 5U, 21U, 0U, 0U, 0U
    };

    return
        expect_normalized_payload(
            TDH_INTYPE_MANIFEST_COUNTEDSTRING,
            counted_unicode,
            (ULONG)sizeof(counted_unicode),
            8U,
            TDH_INTYPE_UNICODESTRING,
            2U,
            expected_unicode,
            (ULONG)sizeof(expected_unicode)
        ) &&
        expect_normalized_payload(
            TDH_INTYPE_REVERSEDCOUNTEDANSISTRING,
            reversed_ansi,
            (ULONG)sizeof(reversed_ansi),
            8U,
            TDH_INTYPE_ANSISTRING,
            2U,
            expected_ansi,
            (ULONG)sizeof(expected_ansi)
        ) &&
        expect_normalized_payload(
            TDH_INTYPE_MANIFEST_COUNTEDBINARY,
            counted_binary,
            (ULONG)sizeof(counted_binary),
            8U,
            TDH_INTYPE_BINARY,
            2U,
            expected_binary,
            (ULONG)sizeof(expected_binary)
        ) &&
        expect_normalized_payload(
            TDH_INTYPE_HEXDUMP,
            hex_dump,
            (ULONG)sizeof(hex_dump),
            8U,
            TDH_INTYPE_HEXDUMP,
            4U,
            expected_hex,
            (ULONG)sizeof(expected_hex)
        ) &&
        expect_normalized_payload(
            TDH_INTYPE_NONNULLTERMINATEDSTRING,
            nonterminated,
            (ULONG)sizeof(nonterminated),
            8U,
            TDH_INTYPE_UNICODESTRING,
            0U,
            nonterminated,
            (ULONG)sizeof(nonterminated)
        ) &&
        expect_normalized_payload(
            TDH_INTYPE_WBEMSID,
            wbem_sid,
            (ULONG)sizeof(wbem_sid),
            8U,
            TDH_INTYPE_SID,
            16U,
            expected_sid,
            (ULONG)sizeof(expected_sid)
        );
}

int main(void)
{
    if (!test_payload_normalization()) {
        return 1;
    }

    if (!expect_name(comtrace_etl_input_type_name(TDH_INTYPE_UINT32), L"UINT32") ||
        !expect_name(comtrace_etl_input_type_name(TDH_INTYPE_WBEMSID), L"WBEMSID") ||
        !expect_name(comtrace_etl_input_type_name(TDH_INTYPE_HEXDUMP), L"HEXDUMP") ||
        !expect_name(comtrace_etl_input_type_name(USHRT_MAX), L"UNKNOWN") ||
        !expect_name(comtrace_etl_output_type_name(TDH_OUTTYPE_DATETIME), L"DATETIME") ||
        !expect_name(comtrace_etl_output_type_name(TDH_OUTTYPE_JSON), L"JSON") ||
        !expect_name(comtrace_etl_output_type_name(TDH_OUTTYPE_PKCS7_WITH_TYPE_INFO), L"PKCS7_WITH_TYPE_INFO") ||
        !expect_name(comtrace_etl_output_type_name(TDH_OUTTYPE_NOPRINT), L"NOPRINT") ||
        !expect_name(comtrace_etl_output_type_name(USHRT_MAX), L"UNKNOWN")) {
        return 1;
    }

    wprintf(L"ETW property format tests passed.\n");
    return 0;
}
