# ETL event property search

ComTrace can filter an ETL trace by the value of a top-level event property.

## Syntax

```cmd
ComTrace.exe --search trace.etl --property Text --equals Hello
ComTrace.exe --search trace.etl --property Text --contains ell
ComTrace.exe --search trace.etl --property ProcessId --equals 1234 --provider {GUID}
```

Use exactly one comparison operator with `--property`:

- `--equals <value>` matches the complete formatted value.
- `--contains <text>` matches a substring in an ANSI or Unicode string.

Property names and values are case-sensitive. The property name is limited to
127 characters and the comparison value to 255 characters. Property filters
combine with provider, event ID, process/thread, COM/OLE, CLSID, IID, OID,
lifetime ID, and Activity ID filters using AND semantics.

## Supported scalar values

The property must be a top-level scalar that TDH can read from the event.
Supported values include:

- ANSI and Unicode strings and characters
- Signed and unsigned 8-, 16-, 32-, and 64-bit integers
- Float and double
- Boolean values, formatted as `true` or `false`
- GUID values in braces
- HEXINT32 and HEXINT64 values in `0x` notation
- Pointer and SIZE_T values in `0x` notation
- FILETIME values as their raw 64-bit tick count
- SYSTEMTIME values as `YYYY-MM-DDTHH:mm:ss.fff`

For `--contains`, only ANSI and Unicode strings are considered. ANSI strings
are converted using the active Windows code page. Other scalar types require
`--equals`.

## Unsupported property shapes

Array properties, nested structure members, WBEM XML fragments, custom-schema
properties, binary payloads, and values whose TDH metadata or payload cannot be
read are not matched by this filter. A missing or unsupported property produces
no match; it does not make the whole trace search fail.

Property search does not change ETL payload decoding or CTRA storage. It is a
filter over the original ETW event metadata and property values.
