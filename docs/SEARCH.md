# ETL event property search

ComTrace can filter an ETL trace by the value of a top-level event property or
one indexed element of a top-level array property.

## Syntax

```cmd
ComTrace.exe --search trace.etl --property Text --equals Hello
ComTrace.exe --search trace.etl --property Text --contains ell
ComTrace.exe --search trace.etl --property Values[1] --equals 22
ComTrace.exe --search trace.etl --property Outer.Inner.NestedCode --equals 1432778632
ComTrace.exe --search trace.etl --property ProcessId --equals 1234 --provider {GUID}
```

Use exactly one comparison operator with `--property`:

- `--equals <value>` matches the complete formatted value.
- `--contains <text>` matches a substring in an ANSI or Unicode string.
- `--greater-than <number>` matches numeric values greater than the supplied number.
- `--greater-or-equal <number>` matches numeric values greater than or equal to the supplied number.
- `--less-than <number>` matches numeric values less than the supplied number.
- `--less-or-equal <number>` matches numeric values less than or equal to the supplied number.

Property names and string values are case-sensitive. The property name is limited to
127 characters and the comparison value to 255 characters. Array indices use
zero-based notation from 0 through 4095, for example `Values[1]`. String payloads
longer than 1023 characters are not matched. Numeric comparisons use the property's
native signed, unsigned, or floating-point type; integer comparisons preserve
64-bit precision. Hexadecimal, pointer, and SIZE_T values use `0x` notation. Property filters
combine with provider, event ID, process/thread, COM/OLE, CLSID, IID, OID,
lifetime ID, and Activity ID filters using AND semantics.

## Supported scalar values

The property can be a top-level scalar, an indexed element of a top-level
array, or a scalar member reached through dot notation in a non-array struct.
For example, `Outer.Inner.Code` selects a nested member. Struct arrays and array
members inside struct parents are not currently supported. An array property
without an index is not matched.
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
- SID values as standard SDDL strings such as `S-1-5-32-544`
- IPv4 values in dotted-decimal notation
- IPv6 binary values in canonical address notation when the schema declares `win:IPv6`

For `--contains`, only ANSI and Unicode strings are considered. ANSI strings
are converted using the active Windows code page. Binary properties support exact
`--equals hex:<bytes>` matching, where each byte is two hexadecimal digits and the
complete payload must match. Numeric ordering operators
support signed and unsigned integers, FILETIME, floating-point values, HEXINT,
pointer, and SIZE_T properties. GUID, Boolean, character, and time-structure
values support equality only.

## Unsupported property shapes

Array properties, nested structure members, WBEM XML fragments, custom-schema
properties, and values whose TDH metadata or payload cannot be read are not
matched by this filter. Binary properties do not support substring or numeric
ordering comparisons. A missing or unsupported property produces
no match; it does not make the whole trace search fail.

Property search does not change ETL payload decoding or CTRA storage. It is a
filter over the original ETW event metadata and property values.
