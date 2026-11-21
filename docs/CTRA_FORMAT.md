# CTRA File Format - Versions 1 and 2

## 1. Scope

This document defines the on-disk CTRA archive format written by ComTrace Archive.

A CTRA file consists of a fixed-size file header, zero or more event records, and a fixed-size completion footer. Version 2 adds a CRC-32 checksum to the footer. It does not change the file header, record header, or event payload layout used by version 1.

All integer fields are serialized in little-endian byte order unless explicitly stated otherwise. GUID fields are stored as the raw 16-byte Windows GUID memory representation.

The current writer creates version 2 archives. The reader accepts both version 1 and version 2 archives.

## 2. File Header

The file header is exactly 24 bytes in both versions.

| Offset | Size | Type | Field | Required value |
|---:|---:|---|---|---|
| 0 | 8 | bytes | Magic | `CTRACEA1` |
| 8 | 4 | u32 | Version | 1 or 2 |
| 12 | 4 | u32 | HeaderSize | 24 |
| 16 | 4 | u32 | Flags | 1 |
| 20 | 4 | u32 | Reserved | 0 |

Readers must reject a file header if the magic, version, header size, flags, or reserved value is invalid. The version in the footer must match the version in the file header.

## 3. Record Structure

Each record starts with a 24-byte record header, followed by its payload. The record layout is identical in versions 1 and 2.

### 3.1. Record Header

| Offset | Size | Type | Field | Required value |
|---:|---:|---|---|---|
| 0 | 4 | u32 | RecordType | 1 (Event) |
| 4 | 4 | u32 | Flags | 0 |
| 8 | 8 | u64 | Sequence | Starts at 1 and increments by 1 |
| 16 | 8 | u64 | PayloadSize | Payload size in bytes |

Version 1 and version 2 define record type 1 as an event record. Other record types and nonzero record flags are invalid.

Sequence numbers must be contiguous and start at one.

### 3.2. Event Payload

An event payload contains an 88-byte metadata block, zero or more extended-data items, and the event user data.

### 3.3. Event Metadata

The metadata block is exactly 88 bytes. The first 80 bytes contain the serialized Windows `EVENT_HEADER`.

| Offset | Size | Type | Field |
|---:|---:|---|---|
| 0 | 2 | u16 | Size |
| 2 | 2 | u16 | HeaderType |
| 4 | 2 | u16 | Flags |
| 6 | 2 | u16 | EventProperty |
| 8 | 4 | u32 | ThreadId |
| 12 | 4 | u32 | ProcessId |
| 16 | 8 | u64 | TimeStamp |
| 24 | 16 | bytes | ProviderId |
| 40 | 2 | u16 | EventDescriptor.Id |
| 42 | 1 | u8 | EventDescriptor.Version |
| 43 | 1 | u8 | EventDescriptor.Channel |
| 44 | 1 | u8 | EventDescriptor.Level |
| 45 | 1 | u8 | EventDescriptor.Opcode |
| 46 | 2 | u16 | EventDescriptor.Task |
| 48 | 8 | u64 | EventDescriptor.Keyword |
| 56 | 4 | u32 | KernelTime |
| 60 | 4 | u32 | UserTime |
| 64 | 16 | bytes | ActivityId |
| 80 | 1 | u8 | BufferContext.ProcessorNumber |
| 81 | 1 | u8 | BufferContext.Alignment |
| 82 | 2 | u16 | BufferContext.LoggerId |
| 84 | 2 | u16 | ExtendedDataCount |
| 86 | 2 | u16 | UserDataLength |

ProviderId and ActivityId are copied as raw 16-byte Windows GUID representations.

### 3.4. Extended Data Items

The extended-data section contains exactly `ExtendedDataCount` items. Each item consists of an 8-byte header followed immediately by its data.

| Offset within item | Size | Type | Field |
|---:|---:|---|---|
| 0 | 2 | u16 | Reserved1 |
| 2 | 2 | u16 | ExtType |
| 4 | 1 | u8 | Linkage |
| 5 | 1 | u8 | Reserved2 |
| 6 | 2 | u16 | DataSize |
| 8 | DataSize | bytes | Data |

The item header is the serialized prefix of `EVENT_HEADER_EXTENDED_DATA_ITEM`, ending immediately before its `DataPtr` member.

There is no additional alignment or padding between an item header, its data, and the next item.

### 3.5. User Data

After all extended-data items, the payload contains exactly `UserDataLength` bytes of user data. The user-data section is the final part of the event payload.

`PayloadSize = 88 + sum(8 + ExtendedDataItem.DataSize) + UserDataLength`

## 4. Completion Footer

The footer follows the final event record immediately. Both versions use the same footer metadata fields. Version 1 uses a 124-byte footer. Version 2 uses a 128-byte footer to append the checksum.

### 4.1. Common Footer Fields

| Offset | Size | Type | Field |
|---:|---:|---|---|
| 0 | 8 | bytes | Magic: `CTRFTR01` |
| 8 | 4 | u32 | Version: matches the file header |
| 12 | 4 | u32 | FooterSize: 124 for v1, 128 for v2 |
| 16 | 8 | u64 | RecordsWritten |
| 24 | 4 | u32 | EventsLost |
| 28 | 4 | u32 | BuffersLost |
| 32 | 4 | u32 | BuffersWritten |
| 36 | 4 | u32 | BuffersRead |
| 40 | 4 | u32 | ProcessTraceStatus |
| 44 | 4 | u32 | CompletionMarker |
| 48 | 4 | u32 | TraceClockType |
| 52 | 4 | u32 | PointerSize |
| 56 | 4 | u32 | OsVersion |
| 60 | 4 | u32 | ProviderVersion |
| 64 | 4 | u32 | NumberOfProcessors |
| 68 | 4 | u32 | BufferSize |
| 72 | 4 | u32 | TimerResolution |
| 76 | 4 | u32 | MaximumFileSize |
| 80 | 4 | u32 | LogFileMode |
| 84 | 4 | u32 | StartBuffers |
| 88 | 4 | u32 | CpuSpeedMHz |
| 92 | 8 | u64 | StartTime |
| 100 | 8 | u64 | EndTime |
| 108 | 8 | u64 | BootTime |
| 116 | 8 | u64 | PerfFrequency |

A completed archive must have a footer version matching the file header, the correct footer size, ProcessTraceStatus equal to 0, CompletionMarker equal to 1, and RecordsWritten equal to the number of event records in the archive.

### 4.2. Version 1 Footer

The version 1 footer ends after the 8-byte `PerfFrequency` field at offset 116. Its total size is 124 bytes. Version 1 archives do not contain a checksum.

### 4.3. Version 2 Checksum Field

Version 2 appends one 4-byte unsigned checksum field:

| Offset | Size | Type | Field |
|---:|---:|---|---|
| 124 | 4 | u32 | CRC32 |

The version 2 footer is exactly 128 bytes. The checksum is serialized in little-endian byte order.

## 5. Version 2 CRC-32

The version 2 checksum uses the standard reflected CRC-32 algorithm with polynomial `0xEDB88320`, initial state `0xFFFFFFFF`, and final XOR `0xFFFFFFFF`.

The checksum covers every byte in the archive from offset 0 up to, but not including, the four-byte checksum field. This includes:

- The complete 24-byte file header.
- Every record header and event payload.
- The 124-byte footer prefix, including the footer magic and all footer metadata fields.

The checksum field itself is excluded from the calculation. A reader compares the calculated checksum with the value stored at footer offset 124.

The standard test vector is the ASCII byte sequence `123456789`, whose CRC-32 value is `0xCBF43926`.

CRC-32 detects accidental data corruption. It is not a cryptographic hash, digital signature, or authenticity guarantee.

## 6. Record and Footer Validation

A strict reader validates the file header, every record, and the footer.

For each record, the reader requires:

- RecordType equal to 1.
- Record flags equal to 0.
- Sequence equal to the next expected sequence number.
- PayloadSize of at least 88 bytes.
- PayloadSize representable by the reader.
- A complete payload within the file and before the footer.
- A valid extended-data count and item sizes.
- UserDataLength equal to the exact remaining payload size.

The footer must immediately follow the final record. Its version and size must match the file version. RecordsWritten must match the number of records read, ProcessTraceStatus must be zero, and CompletionMarker must be one.

For version 2, the reader also validates the CRC-32 checksum. A checksum mismatch is an archive validation failure. Version 1 archives are validated without a checksum.

Strict mode rejects missing or truncated records, an invalid footer, a checksum mismatch, and trailing bytes after the footer.

## 7. Recovery Reading

Recovery mode is intended to read complete records from an archive that may not have a valid completion footer.

Recovery starts after validating the file header. It reads sequential records and stops when it encounters a missing, malformed, or incomplete record or footer. Only complete records that pass record validation are retained. Recovery does not make an incomplete record valid and does not imply that the original archive completed successfully.

When a version 2 footer is present and its metadata is valid, recovery also checks the CRC-32 checksum. If the checksum is invalid, recovery retains the records already read, reports the stop reason as `checksum invalid`, and marks the footer invalid and the archive incomplete.

A recovered archive may have no valid footer. In that case, footer summary information is unavailable. Recovery is a read-only scan; it does not write or repair an archive.

## 8. Version Compatibility

The current reader supports both versions:

| Property | Version 1 | Version 2 |
|---|---:|---:|
| File header size | 24 bytes | 24 bytes |
| Record header size | 24 bytes | 24 bytes |
| Event metadata size | 88 bytes | 88 bytes |
| Extended-data item header size | 8 bytes | 8 bytes |
| Footer size | 124 bytes | 128 bytes |
| Footer checksum | Not present | CRC-32 at offset 124 |

The current writer creates version 2 archives. Version 1 remains readable for compatibility with existing archives.

Any incompatible change to the header, record, event payload, footer metadata, or checksum interpretation requires a new format version.
