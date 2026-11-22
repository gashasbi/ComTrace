# ComTrace

ComTrace is a native Windows command-line toolkit for capturing, inspecting, filtering, archiving, and comparing ETW traces related to COM and OLE activity.

## Requirements

- Windows 10/11 x64
- Visual Studio 2022 with **Desktop development with C++** and the Windows SDK
- CMake 3.21 or newer
- Ninja only for the VS Code/Ninja workflow

## Build and test

### Visual Studio 2022

Open **x64 Native Tools Command Prompt for VS 2022** in the project folder:

```cmd
cmake --preset windows-vs2022-x64
cmake --build --preset windows-vs2022-x64-release
ctest --preset windows-vs2022-x64-release
```

For Debug, replace `release` with `debug`. Open `out\build\windows-vs2022-x64\ComTrace.sln` to work in Visual Studio.

### VS Code / Ninja

Open **x64 Native Tools Command Prompt for VS 2022** in the project folder:

```cmd
cmake --preset windows-msvc-x64-release
cmake --build --preset windows-msvc-x64-release
ctest --preset windows-msvc-x64-release
```

For Debug, replace `release` with `debug` in all three commands. In VS Code, install the recommended **CMake Tools** and **C/C++** extensions and select the corresponding configure preset.

## Command-line tools

The build produces two executables:

- `ComTrace.exe` captures, inspects, filters, and compares traces.
- `ComTraceArchive.exe` converts ETL files to CTRA archives and inspects or scans CTRA archives.

### ComTrace.exe

```cmd
ComTrace.exe --help
ComTrace.exe --version
ComTrace.exe --self-test
ComTrace.exe --providers
ComTrace.exe --capture 60 trace.etl
ComTrace.exe --inspect trace.etl 100
ComTrace.exe --search trace.etl --com --pid 1234 --max-events 50
ComTrace.exe --compare before.etl after.etl
ComTrace.exe --compare before.etl after.ctra --json
```

**Capture** enables the COM Performance and OLE Performance ETW providers and, when Windows permits a private system logger, Kernel Process start/stop events in the same ETL. If the system logger is unavailable, capture falls back to the COM/OLE providers without process lifecycle events and prints a warning. The duration must be between 1 and 3600 seconds. The output path must end in `.etl` and must not already exist. Depending on the machine's ETW permissions, capture may require an elevated terminal.

**Inspect** processes the full trace and prints a bounded sample of events (100 by default; accepted range 1–10000), event-type counts, COM and OLE operation timelines, Process lifetime generations, and a relationship-graph summary. Event samples and lifecycle bounds use raw ETW clock ticks (`time_ticks`); the sample limit only limits detailed event output, not full-trace analysis.

**ETL property decoding** uses TDH metadata and supports scalar values, fixed and dynamic arrays, and structure members, including dynamic arrays of structures. Nested descriptor paths are handled when runtime metadata exposes them, and nested TraceLogging structures are covered by an ETL fixture. The Windows manifest compiler rejects nested `<struct>` declarations, so that specific manifest shape cannot be used as a manifest fixture. WBEM XML fragments and custom-schema properties remain explicitly unsupported rather than being decoded as ordinary scalar values.

**Search** scans an ETL file and supports these optional filters:

- `--provider <guid>`
- `--event-id <id>`
- `--pid <id>`
- `--tid <id>`
- `--com` or `--ole` (mutually exclusive)
- `--max-events <n>` (1–10000; default 100)

Filters can be combined. For example, `--search trace.etl --provider {B8D6861B-D20F-4EEC-BBAE-87E0DD80602B} --event-id 39 --pid 1234` selects COM Performance events matching all supplied filters.

**Compare** accepts ETL or CTRA input files. It reports event-type count changes, COM object changes keyed by process and OID, Process lifetime generations, changes to available CLSID/IID/IPID identity fields, COM and OLE timeline counts, and per-operation timing summaries. Add `--json` for the versioned JSON report (currently schema version 1); its fields and data types are documented in [COMPARISON_JSON.md](docs/COMPARISON_JSON.md).

### ComTraceArchive.exe

```cmd
ComTraceArchive.exe trace.etl trace.ctra
ComTraceArchive.exe --inspect trace.ctra
ComTraceArchive.exe --recover trace.ctra
ComTraceArchive.exe --help
ComTraceArchive.exe --version
```

The first command converts an ETL trace to the CTRA archive format. New archives use **CTRA version 2** and store a CRC-32 checksum over every byte before the checksum field, including the file header, event records, and footer metadata. `--inspect` validates the archive checksum and reports its event and trace-footer statistics. The reader remains backward-compatible with **CTRA version 1** archives, which do not contain a checksum. CRC32 detects accidental corruption; it is not a cryptographic signature or authenticity guarantee. The complete byte layout, validation rules, and version compatibility details are documented in [CTRA_FORMAT.md](docs/CTRA_FORMAT.md).

`--recover` scans recoverable records and reports the recovered count, stopping offset, and stop reason. When a version 2 checksum is invalid, recovery preserves the records that could be read but reports `checksum invalid` and does not mark the archive as complete. Recovery mode is a read-only scan; it does not write a replacement archive.

## Real ETL validation

Use [the real ETL validation workflow](docs/REAL_TRACE_VALIDATION.md) on a Windows machine to capture or supply a trace with COM and OLE activity, confirm both providers' events and operation timelines, convert ETL to CTRA, verify CRC32, and compare the original trace with the archive round trip. The script is `scripts/validate-real-trace.ps1`; new captures also run `ComTraceOleActivityFixture.exe` to generate OLE clipboard and drag-drop registration traffic. Capturing a new trace requires elevated 64-bit PowerShell; validating an existing ETL does not require elevation.

## COM/OLE analysis and limitations

- Event classification is limited to the provider, event ID, version, and opcode mappings explicitly defined in `src/com_event.c` and `src/ole_event.c`.
- CLSID, IID, IPID, OID, CallTraceId, process/thread IDs, and Activity IDs are kept as distinct fields. A value is reported only when the ETW event schema or event header provides it; ComTrace does not scan arbitrary payload bytes or infer missing identifiers.
- COM start/stop pairing uses matching CallTraceId values first. When both events lack CallTraceId, matching ActivityId, process ID, thread ID, and operation form a second level. When both identifiers are absent, process ID, thread ID, and operation form a weaker third level, reported separately. One-sided or conflicting identifiers remain unmatched.
- OLE operation pairing reports ActivityId matches separately from the weaker process/thread/operation fallback used only when both events lack ActivityId. One-sided or conflicting Activity IDs remain unmatched.
- Relationship graph call direction uses explicit `SourceProcessId`/`SourceThreadId` and `TargetProcessId`/`TargetThreadId` endpoints when both are present. When only the target endpoint is available, the event process/thread is treated as the caller; when only the source endpoint is available, the event process/thread is treated as the callee. Zero process and thread IDs do not create graph nodes. A COM object is associated with a valid target process when provided, otherwise with the event process when valid.
- `ActivityId` and `RelatedActivityId` are retained independently. A related activity remains visible as a node when the event has no primary `ActivityId`, but no relationship edge is invented without both identifiers; identical activity IDs do not create self-edges.
- Kernel Process `Start` and `End` events are decoded using the payload `ProcessId` field and tracked as one-based generations within a trace. `DCStart` and `DCEnd` are kept as start/end-of-trace rundown observations; they are not treated as actual process creation or exit. An unmatched End remains an incomplete generation. Parent IDs are retained as descriptive metadata only because Windows can reuse them. Process lifecycle events are available when Capture can start its private system logger; the COM/OLE-only fallback and traces with missing events can produce incomplete lifecycle data.
- COM object and IPID nodes accumulate the number of events that exposed each identity and the minimum/maximum ETW timestamp observed for it. These values describe the observation window in the trace clock's raw ticks; they are not creation or release times. The COM graph and object comparison key still use process ID and OID rather than the Process generation, and the current COM event mapping has no verified object-release event. Therefore, a reused OID within one process cannot yet be separated into distinct object generations.
- Peer process/thread IDs are read from explicitly named event properties such as `TargetProcessId`, `TargetThreadId`, `SourceProcessId`, and `SourceThreadId`. Executable paths are displayed only when Windows permits the process query.
- The OLE Performance manifest inspected on Windows Server 2022 defines 28 events across 14 Clipboard and Drag & Drop operations, but does not declare event payload templates or data fields. ComTrace also queries TDH runtime metadata for displayed events and preserves any named properties it exposes. DataObject identity is not inferred from raw payload bytes; it is only eligible for extraction when a stable runtime property and its meaning can be verified.
- The COM Performance manifest inspected on the Windows Server 2022 CI runner defines 55 events but no templates or data fields. ComTrace therefore relies on named TDH runtime properties when available; some identifiers, including CLSID, may remain unavailable in real traces. It does not infer them from undocumented payload offsets.
- `--providers` reports the three providers in the CLI inventory. It is not a complete inventory of every provider that capture may enable.
- A registered or enabled provider is not proof that it emitted events. Validate event production using an ETL captured on the target Windows system.
- ETL processing uses Windows ETW and TDH APIs. Build and end-to-end trace validation must be performed on Windows.
- ComTrace is a static trace analysis tool. Its output does not establish that an operation is malicious or vulnerable.
