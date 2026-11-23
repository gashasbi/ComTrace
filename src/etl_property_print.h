#ifndef COMTRACE_ETL_PROPERTY_PRINT_H
#define COMTRACE_ETL_PROPERTY_PRINT_H

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <evntcons.h>

void comtrace_etl_print_event_properties(
    const EVENT_RECORD *event_record,
    ULONG pointer_size
);

#endif
