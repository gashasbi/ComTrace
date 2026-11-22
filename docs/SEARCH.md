# ETL event property search

ComTrace can filter an ETL trace by the value of a top-level event property,
one indexed element of a top-level array property, or a scalar member of an
indexed top-level struct array.

## Syntax

```cmd
ComTrace.exe --search trace.etl --property Text --equals Hello
ComTrace.exe --search trace.etl --property Text --contains ell
ComTrace.exe --search trace.etl --property Values[1] --equals 22
ComTrace.exe --search trace.etl --property Values[1].Code --equals 1432778632
ComTrace.exe --search trace.etl --property Outer.Numbers[1] --equals 47
ComTrace.exe --search trace.etl --property Outer.Inner.NestedCode --equals 1432778632
ComTrace.exe --search trace.etl --property ProcessId --equals 1234 --provider {GUID}
```

Use exactly one comparison operator with `--property`:

- `--equals <value>` matches the complete formatted value.
- `--contains <value>` matches a substring in an ANSI or Unicode string, or a byte sequence in a binary property when supplied as `hex:<bytes>`.
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
array, or a scalar member reached through dot notation in a struct. For example,
`Outer.Inner.Code` selects a nested member. One array index can be used at any
level of a property path, including nested struct arrays and arrays inside
struct parents. Examples include `Values[1].Code`,
`Outer.Inner[1].NestedCode`, and `Outer.Numbers[1]`. Multiple array indices in
one property path are not currently supported. An array property without an
index is not matched.
Supported values include:

- ANSI and Unicode strings and characters
- UTF-8 encoded ANSI strings when the schema declares `win:Utf8`, `win:Xml`, or `win:Json`; JSON and XML payloads are decoded as UTF-8; non-ASCII characters are rendered as `\\uXXXX` escapes so output is independent of the active console code page
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
- IPv4 and IPv6 socket-address binary values as `IP:port` and `[IPv6]:port` when the schema declares `win:SocketAddress`; other address families are retained as hexadecimal bytes
- Binary payloads with exact equality and byte-sequence contains matching using `hex:` notation
- HRESULT, NTSTATUS, ErrorCode, and other hexadecimal output types in `0x` notation

For string `--contains`, ANSI strings are converted using the active Windows code page.
Binary properties support exact `--equals hex:<bytes>` matching and byte-sequence
`--contains hex:<bytes>` matching. Each byte is two hexadecimal digits; exact matching
requires the complete payload, while contains matching succeeds when the sequence occurs
anywhere in the payload. Numeric ordering operators
support signed and unsigned integers, FILETIME, floating-point values, HEXINT,
pointer, and SIZE_T properties. GUID, Boolean, character, and time-structure
values support equality only.

## Unsupported property shapes

Array properties without an index, nested struct arrays, multiple array indices
in one property path, WBEM XML fragments, unsupported custom schemas, and values whose TDH
metadata or payload cannot be read are not matched by this filter. Binary properties
do not support numeric ordering comparisons. Scalar custom-schema properties are
matched when their input and output types have a supported scalar representation. A missing or unsupported property produces
no match; it does not make the whole trace search fail.

Property search does not change ETL payload decoding or CTRA storage. It is a
filter over the original ETW event metadata and property values.
