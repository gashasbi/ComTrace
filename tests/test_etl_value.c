#include "etl_value.h"

#include <stdio.h>

typedef struct {
    const char *name;
    ComTraceValueSizeKind kind;
    size_t value_size;
    size_t pointer_size;
    int expected;
} SizeCase;

static int run_size_case(const SizeCase *test_case)
{
    int result = comtrace_value_size_is_valid(
        test_case->kind,
        test_case->value_size,
        test_case->pointer_size
    );

    if (result != test_case->expected) {
        fprintf(stderr, "FAIL: %s\n", test_case->name);
        return 1;
    }

    return 0;
}

int main(void)
{
    static const SizeCase cases[] = {
        {"INT8 accepts one byte", COMTRACE_VALUE_INT8, 1U, 0U, 1},
        {"INT8 rejects two bytes", COMTRACE_VALUE_INT8, 2U, 0U, 0},
        {"UINT8 accepts one byte", COMTRACE_VALUE_UINT8, 1U, 0U, 1},
        {"UINT8 rejects zero bytes", COMTRACE_VALUE_UINT8, 0U, 0U, 0},
        {"INT16 accepts two bytes", COMTRACE_VALUE_INT16, 2U, 0U, 1},
        {"INT16 rejects one byte", COMTRACE_VALUE_INT16, 1U, 0U, 0},
        {"UINT16 accepts two bytes", COMTRACE_VALUE_UINT16, 2U, 0U, 1},
        {"UINT16 rejects four bytes", COMTRACE_VALUE_UINT16, 4U, 0U, 0},
        {"INT32 accepts four bytes", COMTRACE_VALUE_INT32, 4U, 0U, 1},
        {"INT32 rejects eight bytes", COMTRACE_VALUE_INT32, 8U, 0U, 0},
        {"UINT32 accepts four bytes", COMTRACE_VALUE_UINT32, 4U, 0U, 1},
        {"UINT32 rejects two bytes", COMTRACE_VALUE_UINT32, 2U, 0U, 0},
        {"INT64 accepts eight bytes", COMTRACE_VALUE_INT64, 8U, 0U, 1},
        {"INT64 rejects four bytes", COMTRACE_VALUE_INT64, 4U, 0U, 0},
        {"UINT64 accepts eight bytes", COMTRACE_VALUE_UINT64, 8U, 0U, 1},
        {"UINT64 rejects sixteen bytes", COMTRACE_VALUE_UINT64, 16U, 0U, 0},
        {"FLOAT accepts four bytes", COMTRACE_VALUE_FLOAT, 4U, 0U, 1},
        {"FLOAT rejects eight bytes", COMTRACE_VALUE_FLOAT, 8U, 0U, 0},
        {"DOUBLE accepts eight bytes", COMTRACE_VALUE_DOUBLE, 8U, 0U, 1},
        {"DOUBLE rejects four bytes", COMTRACE_VALUE_DOUBLE, 4U, 0U, 0},
        {"BOOLEAN accepts one byte", COMTRACE_VALUE_BOOLEAN, 1U, 0U, 1},
        {"BOOLEAN accepts two bytes", COMTRACE_VALUE_BOOLEAN, 2U, 0U, 1},
        {"BOOLEAN accepts four bytes", COMTRACE_VALUE_BOOLEAN, 4U, 0U, 1},
        {"BOOLEAN accepts eight bytes", COMTRACE_VALUE_BOOLEAN, 8U, 0U, 1},
        {"BOOLEAN rejects three bytes", COMTRACE_VALUE_BOOLEAN, 3U, 0U, 0},
        {"GUID accepts sixteen bytes", COMTRACE_VALUE_GUID, 16U, 0U, 1},
        {"GUID rejects eight bytes", COMTRACE_VALUE_GUID, 8U, 0U, 0},
        {"FILETIME accepts eight bytes", COMTRACE_VALUE_FILETIME, 8U, 0U, 1},
        {"FILETIME rejects four bytes", COMTRACE_VALUE_FILETIME, 4U, 0U, 0},
        {"HEXINT32 accepts four bytes", COMTRACE_VALUE_HEXINT32, 4U, 0U, 1},
        {"HEXINT32 rejects eight bytes", COMTRACE_VALUE_HEXINT32, 8U, 0U, 0},
        {"HEXINT64 accepts eight bytes", COMTRACE_VALUE_HEXINT64, 8U, 0U, 1},
        {"HEXINT64 rejects four bytes", COMTRACE_VALUE_HEXINT64, 4U, 0U, 0},
        {"POINTER accepts 4-byte trace", COMTRACE_VALUE_POINTER, 4U, 4U, 1},
        {"POINTER rejects 8 bytes in 4-byte trace", COMTRACE_VALUE_POINTER, 8U, 4U, 0},
        {"POINTER accepts 8-byte trace", COMTRACE_VALUE_POINTER, 8U, 8U, 1},
        {"POINTER rejects 4 bytes in 8-byte trace", COMTRACE_VALUE_POINTER, 4U, 8U, 0},
        {"SIZET accepts 4-byte trace", COMTRACE_VALUE_SIZET, 4U, 4U, 1},
        {"SIZET rejects 8 bytes in 4-byte trace", COMTRACE_VALUE_SIZET, 8U, 4U, 0},
        {"SIZET accepts 8-byte trace", COMTRACE_VALUE_SIZET, 8U, 8U, 1},
        {"SIZET rejects 4 bytes in 8-byte trace", COMTRACE_VALUE_SIZET, 4U, 8U, 0},
        {"SIZET rejects zero pointer size", COMTRACE_VALUE_SIZET, 0U, 0U, 0},
        {"POINTER rejects zero pointer size", COMTRACE_VALUE_POINTER, 0U, 0U, 0},
        {"SYSTEMTIME accepts sixteen bytes", COMTRACE_VALUE_SYSTEMTIME, 16U, 0U, 1},
        {"SYSTEMTIME rejects eight bytes", COMTRACE_VALUE_SYSTEMTIME, 8U, 0U, 0},
        {"Unknown value kind is rejected", (ComTraceValueSizeKind)999, 4U, 0U, 0}
    };

    static const unsigned char ansi_terminated[] = {'a', 'b', 0U, 'c'};
    static const unsigned char ansi_unterminated[] = {'a', 'b', 'c'};
    static const unsigned char ansi_long[] = {'a', 'b', 'c', 'd', 'e', 'f'};
    static const unsigned char ansi_after_limit[] = {'a', 'b', 'c', 'd', 0U};
    static const unsigned char unicode_terminated[] = { 'a', 0U, 'b', 0U, 0U, 0U };
    static const unsigned char unicode_unterminated[] = { 'a', 0U, 'b', 0U };
    static const unsigned char unicode_long[] = {
        'a', 0U, 'b', 0U, 'c', 0U, 'd', 0U
    };
    static const unsigned char systemtime_bytes[] = {0xEA,0x07,0x0B,0,0,0,0x0F,0,0x16,0,0x2D,0,0x1E,0,0x7B,0};
    ComTraceSystemTime decoded_time;
    size_t content_size;
    size_t displayed_size;
    int terminated;
    size_t index;
    size_t payload_offset;
    size_t payload_size;
    static const unsigned char counted_little_endian[] = {
        4U, 0U, 0xDEU, 0xADU, 0xBEU, 0xEFU
    };
    static const unsigned char counted_big_endian[] = {
        0U, 4U, 0xDEU, 0xADU, 0xBEU, 0xEFU
    };
    static const unsigned char counted_invalid[] = {
        5U, 0U, 0xDEU, 0xADU, 0xBEU, 0xEFU
    };
    static const unsigned char hexdump_value[] = {
        4U, 0U, 0U, 0U, 0x11U, 0x22U, 0x33U, 0x44U
    };
    int failures = 0;

    for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
        failures += run_size_case(&cases[index]);
    }

    if (!comtrace_decode_systemtime(systemtime_bytes, sizeof(systemtime_bytes), &decoded_time) || decoded_time.year != 2026U || decoded_time.month != 11U || decoded_time.day_of_week != 0U || decoded_time.day != 15U || decoded_time.hour != 22U || decoded_time.minute != 45U || decoded_time.second != 30U || decoded_time.milliseconds != 123U) {
        fprintf(stderr, "FAIL: SYSTEMTIME fields are decoded correctly");
        ++failures;
    }

    if (comtrace_decode_systemtime(systemtime_bytes, 8U, &decoded_time) || comtrace_decode_systemtime(NULL, sizeof(systemtime_bytes), &decoded_time) || comtrace_decode_systemtime(systemtime_bytes, sizeof(systemtime_bytes), NULL)) {
        fprintf(stderr, "FAIL: SYSTEMTIME rejects invalid input");
        ++failures;
    }

    if (!comtrace_scan_string(
            ansi_terminated, sizeof(ansi_terminated), 1U, 256U,
            &content_size, &displayed_size, &terminated) ||
        content_size != 2U || displayed_size != 2U || !terminated) {
        fprintf(stderr, "FAIL: ANSI terminated string\n");
        ++failures;
    }

    if (!comtrace_scan_string(
            ansi_unterminated, sizeof(ansi_unterminated), 1U, 256U,
            &content_size, &displayed_size, &terminated) ||
        content_size != 3U || displayed_size != 3U || terminated) {
        fprintf(stderr, "FAIL: ANSI unterminated string\n");
        ++failures;
    }

    if (!comtrace_scan_string(
            unicode_terminated, sizeof(unicode_terminated), 2U, 256U,
            &content_size, &displayed_size, &terminated) ||
        content_size != 4U || displayed_size != 4U || !terminated) {
        fprintf(stderr, "FAIL: Unicode terminated string\n");
        ++failures;
    }

    if (!comtrace_scan_string(
            unicode_unterminated, sizeof(unicode_unterminated), 2U, 256U,
            &content_size, &displayed_size, &terminated) ||
        content_size != 4U || displayed_size != 4U || terminated) {
        fprintf(stderr, "FAIL: Unicode unterminated string\n");
        ++failures;
    }

    if (comtrace_scan_string(
            unicode_unterminated, sizeof(unicode_unterminated), 2U, 256U,
            NULL, &displayed_size, &terminated)) {
        fprintf(stderr, "FAIL: Null output pointer rejected\n");
        ++failures;
    }

    if (comtrace_scan_string(
            unicode_unterminated, sizeof(unicode_unterminated) - 1U, 2U, 256U,
            &content_size, &displayed_size, &terminated)) {
        fprintf(stderr, "FAIL: Odd Unicode byte count rejected\n");
        ++failures;
    }

    if (comtrace_scan_string(
            ansi_unterminated, sizeof(ansi_unterminated), 4U, 256U,
            &content_size, &displayed_size, &terminated)) {
        fprintf(stderr, "FAIL: Unsupported string unit rejected\n");
        ++failures;
    }

    if (!comtrace_scan_string(
            ansi_long, sizeof(ansi_long), 1U, 3U,
            &content_size, &displayed_size, &terminated) ||
        content_size != 6U || displayed_size != 3U || terminated) {
        fprintf(stderr, "FAIL: ANSI display limit\n");
        ++failures;
    }

    if (!comtrace_scan_string(
            ansi_after_limit, sizeof(ansi_after_limit), 1U, 2U,
            &content_size, &displayed_size, &terminated) ||
        content_size != 4U || displayed_size != 2U || !terminated) {
        fprintf(stderr, "FAIL: Terminator after display limit\n");
        ++failures;
    }

    if (!comtrace_scan_string(
            ansi_long, sizeof(ansi_long), 1U, 2U,
            &content_size, &displayed_size, &terminated) ||
        content_size != 6U || displayed_size != 2U || terminated) {
        fprintf(stderr, "FAIL: Display truncation\n");
        ++failures;
    }

    if (!comtrace_scan_string(
            unicode_long, sizeof(unicode_long), 2U, 5U,
            &content_size, &displayed_size, &terminated) ||
        content_size != 8U || displayed_size != 4U || terminated) {
        fprintf(stderr, "FAIL: Unicode aligned display limit\n");
        ++failures;
    }

    if (!comtrace_scan_string(
            ansi_unterminated, sizeof(ansi_unterminated), 1U, 0U,
            &content_size, &displayed_size, &terminated) ||
        content_size != 3U || displayed_size != 0U || terminated) {
        fprintf(stderr, "FAIL: Zero display limit\n");
        ++failures;
    }

    if (comtrace_scan_string(
            NULL, 0U, 1U, 256U,
            &content_size, &displayed_size, &terminated)) {
        fprintf(stderr, "FAIL: Null string input rejected\n");
        ++failures;
    }

    if (comtrace_scan_string(
            ansi_terminated, sizeof(ansi_terminated), 1U, 256U,
            &content_size, NULL, &terminated)) {
        fprintf(stderr, "FAIL: Null displayed-size output rejected\n");
        ++failures;
    }

    if (comtrace_scan_string(
            ansi_terminated, sizeof(ansi_terminated), 1U, 256U,
            &content_size, &displayed_size, NULL)) {
        fprintf(stderr, "FAIL: Null terminated output rejected\n");
        ++failures;
    }

    if (!comtrace_scan_string(
            ansi_terminated, 0U, 1U, 256U,
            &content_size, &displayed_size, &terminated) ||
        content_size != 0U || displayed_size != 0U || terminated) {
        fprintf(stderr, "FAIL: Empty string buffer\n");
        ++failures;
    }

    if (!comtrace_decode_length_prefixed_payload(
            counted_little_endian, sizeof(counted_little_endian), 2U, 0,
            &payload_offset, &payload_size
        ) || payload_offset != 2U || payload_size != 4U ||
        counted_little_endian[payload_offset] != 0xDEU) {
        fprintf(stderr, "FAIL: Little-endian counted payload framing\n");
        ++failures;
    }

    if (!comtrace_decode_length_prefixed_payload(
            counted_big_endian, sizeof(counted_big_endian), 2U, 1,
            &payload_offset, &payload_size
        ) || payload_offset != 2U || payload_size != 4U ||
        counted_big_endian[payload_offset] != 0xDEU) {
        fprintf(stderr, "FAIL: Reversed counted payload framing\n");
        ++failures;
    }

    if (comtrace_decode_length_prefixed_payload(
            counted_invalid, sizeof(counted_invalid), 2U, 0,
            &payload_offset, &payload_size
        )) {
        fprintf(stderr, "FAIL: Counted payload rejects lengths beyond its buffer\n");
        ++failures;
    }

    if (!comtrace_decode_length_prefixed_payload(
            hexdump_value, sizeof(hexdump_value), 4U, 0,
            &payload_offset, &payload_size
        ) || payload_offset != 4U || payload_size != 4U ||
        hexdump_value[payload_offset] != 0x11U) {
        fprintf(stderr, "FAIL: Hexdump length framing\n");
        ++failures;
    }

    if (!comtrace_decode_wbem_sid_payload(
            32U, 8U, &payload_offset, &payload_size
        ) || payload_offset != 16U || payload_size != 16U) {
        fprintf(stderr, "FAIL: 64-bit WBEM SID layout\n");
        ++failures;
    }

    if (!comtrace_decode_wbem_sid_payload(
            24U, 4U, &payload_offset, &payload_size
        ) || payload_offset != 8U || payload_size != 16U) {
        fprintf(stderr, "FAIL: 32-bit WBEM SID layout\n");
        ++failures;
    }

    if (comtrace_decode_wbem_sid_payload(
            16U, 8U, &payload_offset, &payload_size
        ) || comtrace_decode_wbem_sid_payload(
            32U, 16U, &payload_offset, &payload_size
        )) {
        fprintf(stderr, "FAIL: Invalid WBEM SID layouts are rejected\n");
        ++failures;
    }

    if (failures != 0) {
        fprintf(stderr, "%d test(s) failed\n", failures);
        return 1;
    }

    printf("%zu size cases and 16 string cases passed\n",
        sizeof(cases) / sizeof(cases[0]));
    return 0;
}
