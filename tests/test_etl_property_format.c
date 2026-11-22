#include "etl_property_format.h"

#include <stdio.h>
#include <wchar.h>

static int expect_name(const wchar_t *actual, const wchar_t *expected)
{
    if (wcscmp(actual, expected) != 0) {
        fwprintf(stderr, L"Expected %ls, got %ls\n", expected, actual);
        return 0;
    }
    return 1;
}

int main(void)
{
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
