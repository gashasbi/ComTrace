# ETL Property Decoder

ComTrace reads ETW event schemas through TDH and keeps schema traversal, property retrieval, and value formatting in separate modules:

- `src/etl_decoder.c` retrieves and validates `TRACE_EVENT_INFO`, resolves nested property paths, reads property bytes, and resolves dynamic array counts.
- `src/etl_property_format.c` formats TDH output types, value/bitmap maps, pattern-map metadata, and custom-schema metadata.
- `src/etl_property_print.c` renders decoded event property values for the CLI. It owns property traversal output, display formatting, and per-property status reporting; the ETL inspection coordinator calls one public entry point.
- `src/etl_inspect.c` coordinates trace inspection and delegates event-property traversal and CLI output to `src/etl_property_print.c`.

## Property shapes

The decoder supports scalar properties, fixed arrays, dynamic arrays, structures, arrays of structures, and nested structures when TDH exposes the corresponding metadata. Property paths retain struct and array indexes, for example `Outer[0].Members[2]`.

Dynamic array counts can be supplied by signed or unsigned 8-, 16-, 32-, or 64-bit integer fields, hexadecimal integer fields, or pointer-sized fields. Negative counts are rejected. Count fields must be scalar integer properties.

The decoder enforces these resource limits:

| Resource | Limit |
| --- | ---: |
| Nested structure depth | 16 levels |
| Array elements | 4096 |
| Event metadata | 1 MiB |
| One property payload | 16 MiB |

Legacy fixed-array metadata is also recognized when the metadata reports a count greater than one without the newer fixed-count flag. A one-element array from older metadata cannot always be distinguished from a scalar property.

## Input and output types

All documented TDH input type identifiers are named in inspection output. The formatter handles the common scalar types, strings, GUIDs, SID values, pointers, pointer-sized integers, FILETIME, SYSTEMTIME, IPv4/IPv6, socket addresses, HRESULT, Win32 errors, NTSTATUS, ports, and hexadecimal integer formats.

Counted, reversed-counted, manifest-counted, and non-null-terminated string payloads are normalized before display. Manifest-counted binary and WBEM hexadecimal-dump payloads have their length prefixes removed before raw-byte output. WBEM SID payloads skip the embedded `TOKEN_USER` prefix using the trace pointer size.

TDH formatting is used for date/time output types, reduced strings, PKCS7-with-type-info data, and numeric values requested as strings when TDH can format them. A successful TDH-formatted value also includes its original raw bytes. `NoPrint` fields remain suppressed.

Unknown or reserved input types are preserved as hexadecimal bytes instead of being interpreted as an unrelated scalar type.

## Maps

Value maps and bitmap maps are decoded from `EVENT_MAP_INFO` entries. Inspection reports the map kind and name, resolved label, and original raw value. Bitmap output retains unknown bits as hexadecimal values.

WMI value maps that use string inputs are matched against the property's normalized ANSI or Unicode string. WMI `Values` and `BitValues` qualifiers without an explicit map are handled as index-based value maps or bitmaps.

Pattern maps are message-string transformations, not property-value maps. When TDH exposes one, ComTrace reports its format and entries as metadata and does not apply it to a property value.

Property search compares the underlying typed value, not the rendered value-map label. Use the numeric value for mapped integer and bit fields.

## WBEM XML and custom schemas

Properties marked as WBEM XML fragments retain that format marker. String payloads can be displayed and searched as text; binary payloads remain raw bytes. ComTrace does not infer an XML schema or turn arbitrary XML text into unrelated scalar fields.

Properties marked with `PropertyHasCustomSchema` expose the custom protocol identifier and schema bytes, and preserve the event payload as raw hexadecimal data. Custom-schema protocols are provider-defined; protocol-specific deserialization is intentionally not guessed. A decoder for a particular custom protocol must be implemented against that protocol's documented schema.

## Fixture coverage

The ETW fixtures cover dynamic and fixed arrays, signed count fields, nested dynamic arrays inside structures, counted and reversed strings, non-null-terminated strings, WBEM SID and hexadecimal-dump normalization, nested TraceLogging structures, TraceLogging custom-schema payloads, field tags, value maps, bitmap maps, common scalar input types, date/time and pointer values, string output formats, reduced strings, ETW time, no-print fields, and PKCS7-with-type-info fallback to raw bytes.

The Windows manifest compiler rejects nested `<struct>` declarations, so nested TraceLogging structures are used for that fixture shape. WBEM-only encodings and provider-specific custom protocols still require real provider schemas for full end-to-end validation.

## Microsoft references

- [Retrieving Event Data Using TDH](https://learn.microsoft.com/en-us/windows/win32/etw/retrieving-event-data-using-tdh)
- [Using TdhGetProperty to Consume Event Data](https://learn.microsoft.com/en-us/windows/win32/etw/using-tdhgetproperty-to-consume-event-data)
- [MAP_FLAGS enumeration](https://learn.microsoft.com/en-us/windows/win32/api/tdh/ne-tdh-map_flags)
- [TdhFormatProperty function](https://learn.microsoft.com/en-us/windows/win32/api/tdh/nf-tdh-tdhformatproperty)
- [Event Manifest Input Types](https://learn.microsoft.com/en-us/windows/desktop/WES/eventmanifestschema-inputtype-complextype)
