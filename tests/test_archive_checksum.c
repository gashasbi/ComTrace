#include <stdio.h>
#include <string.h>
#include "archive_checksum.h"

static int test_standard_vector(void)
{
    static const BYTE input[] = "123456789";

    return comtrace_archive_checksum(
        input,
        sizeof(input) - 1U
    ) == 0xCBF43926UL;
}

static int test_incremental_update(void)
{
    static const BYTE input[] = "123456789";
    ULONG state;

    state = comtrace_archive_checksum_begin();
    state = comtrace_archive_checksum_update(state, input, 4U);
    state = comtrace_archive_checksum_update(state, input + 4U, 5U);

    return comtrace_archive_checksum_finish(state) == 0xCBF43926UL;
}

static int test_empty_input(void)
{
    return comtrace_archive_checksum(NULL, 0U) == 0U;
}

static int test_invalid_input(void)
{
    ULONG state = comtrace_archive_checksum_begin();

    return comtrace_archive_checksum_update(state, NULL, 1U) == state;
}

int main(void)
{
    if (!test_standard_vector() ||
        !test_incremental_update() ||
        !test_empty_input() ||
        !test_invalid_input()) {
        fputs("Archive checksum tests failed.\n", stderr);
        return 1;
    }

    puts("Archive checksum tests passed.");
    return 0;
}
