# Archive Performance and Large-File Validation

## Streaming behavior

The archive writer serializes events into a fixed 64 KiB buffer and writes full buffer blocks to disk. The buffer is flushed before the completion footer is finalized and before the file is flushed and closed. CRC-32 is updated in serialization order as bytes enter the buffer, so buffering does not change the checksum coverage or the on-disk format.

The archive reader processes one record at a time. It reads the fixed event metadata first, then allocates one block for that event's extended-data descriptors and serialized payload. The returned event owns that block until `comtrace_archive_event_destroy` is called. Callers should destroy each event before requesting or retaining another event when processing a long archive.

Neither the writer nor reader keeps a collection of all archive events. Memory use therefore depends on the writer buffer, the current event, and caller-owned data, rather than the total number of records in the archive.

## Stress and benchmark runs

The stress executable is built as `ComTraceArchiveStressTests`. Its default run writes and reads 100,000 events with 512-byte payloads, validates every payload and sequence, and checks the version 2 checksum.

Run the default workload through CTest:

```powershell
ctest --test-dir build -C Debug -R '^comtrace_archive_stress$' --output-on-failure
```

Run a custom event count directly:

```powershell
.\build\Debug\ComTraceArchiveStressTests.exe 500000
```

Run the large-file workload directly:

```powershell
.\build\Debug\ComTraceArchiveStressTests.exe --large
```

The `--large` workload writes 9,000,000 events. With the current 512-byte test payload and record metadata, the resulting archive is expected to exceed 4 GiB. The test explicitly fails if the resulting file does not exceed the 4 GiB boundary. It then reads every record and validates the footer record count and CRC-32 checksum.

The stress output reports archive size, write/read duration, process CPU time, throughput, and peak process working set. Results are measurements, not fixed pass/fail performance thresholds: disk speed, CPU, memory pressure, Windows version, and build configuration affect the numbers.

The large-file workload requires several gigabytes of free disk space and may take substantially longer than the default CTest workload. It is intentionally not part of the default automated test run.

## Format and file-size limits

CTRA file offsets, file sizes, record sequence numbers, and record payload sizes use 64-bit values. File I/O is issued in Windows `DWORD`-sized chunks, while the reader validates that an individual event payload is representable by the process address space before allocating it. This permits archives larger than 4 GiB without requiring the complete archive to be mapped or loaded into memory.

An individual event remains bounded by the CTRA field widths and available address space. Large archive support does not imply that a single event can consume arbitrary memory.
