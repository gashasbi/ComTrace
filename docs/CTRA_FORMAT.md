# CTRA File Format - Version 1

## 1. Scope

This document defines the on-disk format written by ComTrace Archive version 1.

A CTRA file consists of a fixed-size file header, zero or more event records, and a fixed-size completion footer.

All integer fields are serialized in little-endian byte order unless explicitly stated otherwise. GUID fields are stored as the raw 16-byte Windows GUID memory representation.

## 2. File Header

The file header is exactly 24 bytes.

| Offset | Size | Type | Field | Required value |
|---:|---:|---|---|---|
| 0 | 8 | bytes | Magic | `CTRACEA1` |
| 8 | 4 | u32 | Version | 1 |
| 12 | 4 | u32 | HeaderSize | 24 |
| 16 | 4 | u32 | Flags | 1 |
| 20 | 4 | u32 | Reserved | 0 |

Readers must reject a file header if any required value does not match.

## 3. Record Structure

Each record starts with a 24-byte record header, followed by its payload.

### 3.1 Record Header

| Offset | Size | Type | Field | Required value |
|---:|---:|---|---|---|
| 0 | 4 | u32 | RecordType | 1 (Event) |
| 4 | 4 | u32 | Flags | 0 |
| 8 | 8 | u64 | Sequence | Starts at 1, increments by 1 |
| 16 | 8 | u64 | PayloadSize | Payload size in bytes |

Version 1 defines record type 1 as an event record. Other record types and nonzero record flags are invalid in version 1.

Sequence numbers must be contiguous and start at one.

### 3.2 Event Payload

An event payload contains an 88-byte metadata block, zero or more extended-data items, and the event user data.

### 3.3 Event Metadata

The metadata block is exactly 88 bytes. The first 80 bytes contain the serialized Windows EVENT_HEADER.

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

### 3.4 Extended Data Items

The extended-data section contains exactly ExtendedDataCount items. Each item consists of an 8-byte header followed immediately by its data.

| Offset within item | Size | Type | Field |
|---:|---:|---|---|
| 0 | 2 | u16 | Reserved1 |
| 2 | 2 | u16 | ExtType |
| 4 | 1 | u8 | Linkage |
| 5 | 1 | u8 | Reserved2 |
| 6 | 2 | u16 | DataSize |
| 8 | DataSize | bytes | Data |

The item header is the serialized prefix of EVENT_HEADER_EXTENDED_DATA_ITEM, ending immediately before its DataPtr member.

There is no additional alignment or padding between an item header, its data, and the next item.

### 3.5 User Data

After all extended-data items, the payload contains exactly UserDataLength bytes of user data. The user-data section is the final part of the event payload.

PayloadSize = 88 + sum(8 + ExtendedDataItem.DataSize) + UserDataLength

## 4. Completion Footer

The footer is exactly 124 bytes and follows the final event record immediately.

| Offset | Size | Type | Field |
|---:|---:|---|---|
| 0 | 8 | bytes | Magic: CTRFTR01 |
| 8 | 4 | u32 | Version: 1 |
| 12 | 4 | u32 | FooterSize: 124 |
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

A completed archive must have version 1, footer size 124, ProcessTraceStatus equal to 0, and CompletionMarker equal to 1. RecordsWritten must equal the number of event records in the archive.

## 5. Record and Footer Validation

A strict reader validates the file header, every record, and the footer.

For each record, the reader requires:

- RecordType equal to 1.
- Record flags equal to 0.
- Sequence equal to the next expected sequence number.
- PayloadSize of at least 88 bytes.
- PayloadSize representable by the reader.
- A complete payload within the file.
- A valid extended-data count and item sizes.
- UserDataLength equal to the exact remaining payload size.

The footer must immediately follow the final record. Strict mode rejects missing or truncated records, an invalid footer, and trailing bytes after the footer.

## 6. Recovery Reading

Recovery mode is intended to read complete records from an archive that may not have a valid completion footer.

Recovery starts after validating the file header. It reads sequential records and stops when it encounters a missing, malformed, or incomplete record or footer.

Only complete records that pass record validation are retained. Recovery does not make an incomplete record valid and does not imply that the original archive completed successfully.

A recovered archive may have no valid footer. In that case, footer summary information is unavailable.

## 7. Version 1 Compatibility

The following values are fixed by this format version:

- File header size: 24 bytes.
- Record header size: 24 bytes.
- Event metadata size: 88 bytes.
- Extended-data item header size: 8 bytes.
- Footer size: 124 bytes.
- File format version: 1.
- Footer version: 1.
- Event record type: 1.

Readers must reject unsupported header versions, footer versions, record types, and reserved or flag values that violate the rules defined above.

Any incompatible change to these structures or their interpretation requires a new format version.
