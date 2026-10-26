#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COMTRACE_VERSION "0.1.0"

static void print_usage(const char *program_name)
{
    (void)printf("Usage: %s [--help | --version | --self-test]\n", program_name);
}

int main(int argc, char **argv)
{
    if (argc == 2) {
        if (strcmp(argv[1], "--help") == 0) {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        }

        if (strcmp(argv[1], "--version") == 0) {
            (void)printf("ComTrace %s\n", COMTRACE_VERSION);
            return EXIT_SUCCESS;
        }

        if (strcmp(argv[1], "--self-test") == 0) {
            (void)puts("ComTrace self-test: PASS");
            return EXIT_SUCCESS;
        }
    }

    if (argc > 1) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    (void)printf("ComTrace %s\n", COMTRACE_VERSION);
    (void)puts("Native Windows COM/OLE tracing toolkit");
    (void)puts("Runtime capture is not implemented in this scaffold.");
    return EXIT_SUCCESS;
}
