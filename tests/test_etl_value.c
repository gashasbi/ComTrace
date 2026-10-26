#include "etl_value.h"

#include <stdio.h>

static int check(
    const char *name,
    const unsigned char *value,
    size_t value_size,
    size_t unit_size,
    size_t display_limit,
    int expected_result,
    size_t expected_content,
    size_t expected_displayed,
    int expected_terminated
)
{
    size_t content_size = 999U;
    size_t displayed_size = 999U;
    int terminated = -1;
    int result = comtrace_scan_string(
        value,
        value_size,
        unit_size,
        display_limit,
        &content_size,
        &displayed_size,
        &terminated
    );

    if (result != expected_result ||
        (result != 0 &&
         (content_size != expected_content ||
          displayed_size != expected_displayed ||
          terminated != expected_terminated))) {
        (void)fprintf(stderr, "FAIL: %s\n", name);
        return 0;
    }

    (void)printf("PASS: %s\n", name);
    return 1;
}

int main(void)
{
    static const unsigned char ansi_terminated[] = {'a', 'b', 0};
    static const unsigned char ansi_unterminated[] = {'a', 'b', 'c'};
    static const unsigned char ansi_after_limit[] = {
        'a', 'b', 'c', 'd', 0
    };
    static const unsigned char unicode_terminated[] = {
        'A', 0, 'B', 0, 0, 0
    };
    static const unsigned char unicode_unterminated[] = {
        'A', 0, 'B', 0
    };
    static const unsigned char long_ansi[] = {
        'a', 'b', 'c', 'd', 'e', 'f'
    };
    static const unsigned char odd_unicode[] = {'A', 0, 'B'};
    int passed = 1;

    passed &= check(
        "ANSI terminated", ansi_terminated, sizeof(ansi_terminated),
        1U, 256U, 1, 2U, 2U, 1
    );
    passed &= check(
        "ANSI unterminated", ansi_unterminated, sizeof(ansi_unterminated),
        1U, 256U, 1, 3U, 3U, 0
    );
    passed &= check(
        "Terminator after display limit", ansi_after_limit,
        sizeof(ansi_after_limit), 1U, 2U, 1, 4U, 2U, 1
    );
    passed &= check(
        "Unicode terminated", unicode_terminated,
        sizeof(unicode_terminated), 2U, 256U, 1, 4U, 4U, 1
    );
    passed &= check(
        "Unicode unterminated", unicode_unterminated,
        sizeof(unicode_unterminated), 2U, 256U, 1, 4U, 4U, 0
    );
    passed &= check(
        "Unicode aligned display limit", long_ansi, sizeof(long_ansi),
        2U, 5U, 1, 6U, 4U, 0
    );
    passed &= check(
        "Display truncation", long_ansi, sizeof(long_ansi),
        1U, 4U, 1, 6U, 4U, 0
    );
    passed &= check(
        "Zero display limit", ansi_unterminated,
        sizeof(ansi_unterminated), 1U, 0U, 1, 3U, 0U, 0
    );
    passed &= check(
        "Reject odd Unicode size", odd_unicode, sizeof(odd_unicode),
        2U, 256U, 0, 0U, 0U, 0
    );
    passed &= check(
        "Reject unsupported unit size", ansi_unterminated,
        sizeof(ansi_unterminated), 4U, 256U, 0, 0U, 0U, 0
    );

    return passed ? 0 : 1;
}