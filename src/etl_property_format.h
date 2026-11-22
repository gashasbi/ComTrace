#ifndef COMTRACE_ETL_PROPERTY_FORMAT_H
#define COMTRACE_ETL_PROPERTY_FORMAT_H

#include "etl_decoder.h"

int comtrace_etl_print_tdh_formatted_property(
    const BYTE *metadata_buffer,
    ULONG metadata_size,
    const EVENT_PROPERTY_INFO *property,
    USHORT in_type,
    USHORT out_type,
    const BYTE *value,
    ULONG value_size,
    ULONG pointer_size
);

int comtrace_etl_print_mapped_property_value(
    const EVENT_RECORD *event_record,
    const BYTE *metadata_buffer,
    ULONG metadata_size,
    const EVENT_PROPERTY_INFO *property,
    USHORT in_type,
    USHORT out_type,
    const BYTE *value,
    ULONG value_size,
    ULONG pointer_size
);

void comtrace_etl_print_custom_schema_metadata(
    const BYTE *metadata_buffer,
    ULONG metadata_size,
    const EVENT_PROPERTY_INFO *property
);

#endif
