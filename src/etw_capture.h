#ifndef COMTRACE_ETW_CAPTURE_H
#define COMTRACE_ETW_CAPTURE_H

#include <wchar.h>

int comtrace_capture_com_perf(
    const wchar_t *duration_text,
    const wchar_t *output_path
);

#endif