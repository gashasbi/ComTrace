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

**Capture** enables the configured COM Performance and OLE Performance ETW providers. The duration must be between 1 and 3600 seconds. The output path must end in `.etl` and must not already exist. Depending on the machine's ETW permissions, capture may require an elevated terminal.

**Inspect** processes the full trace and prints a bounded sample of events (100 by default; accepted range 1–10000), event-type counts, COM and OLE operation timelines, and a relationship-graph summary. The sample limit only limits detailed event output; the trace analysis continues across the full ETL.

**Search** scans an ETL file and supports these optional filters:

- `--provider <guid>`
- `--event-id <id>`
- `--pid <id>`
- `--tid <id>`
- `--com` or `--ole` (mutually exclusive)
- `--max-events <n>` (1–10000; default 100)

Filters can be combined. For example, `--search trace.etl --provider {B8D6861B-D20F-4EEC-BBAE-87E0DD80602B} --event-id 39 --pid 1234` selects COM Performance events matching all supplied filters.

**Compare** accepts ETL or CTRA input files. It reports event-type count changes, COM object changes keyed by process and OID, changes to available CLSID/IID/IPID identity fields, COM and OLE timeline counts, and per-operation timing summaries. Add `--json` for the versioned JSON report (currently schema version 1).

### ComTraceArchive.exe

```cmd
ComTraceArchive.exe trace.etl trace.ctra
ComTraceArchive.exe --inspect trace.ctra
ComTraceArchive.exe --recover trace.ctra
ComTraceArchive.exe --help
ComTraceArchive.exe --version
```

The first command converts an ETL trace to the CTRA archive format. `--inspect` validates an archive and reports its event and trace-footer statistics. `--recover` scans recoverable records and reports the recovered count, stopping offset, and stop reason. Recovery mode is a read-only scan; it does not write a replacement archive.

## COM/OLE analysis and limitations

- Event classification is limited to the provider, event ID, version, and opcode mappings explicitly defined in `src/com_event.c` and `src/ole_event.c`.
- CLSID, IID, IPID, OID, CallTraceId, process/thread IDs, and Activity IDs are kept as distinct fields. A value is reported only when the ETW event schema or event header provides it; ComTrace does not scan arbitrary payload bytes or infer missing identifiers.
- Peer process/thread IDs are read from explicitly named event properties such as `TargetProcessId`, `TargetThreadId`, `SourceProcessId`, and `SourceThreadId`. Executable paths are displayed only when Windows permits the process query.
- The OLE Performance manifest inspected on Windows Server 2022 defines 28 events across 14 Clipboard and Drag & Drop operations, but does not declare event payload templates or data fields. The operation (including `OLE_DragDrop_DragOperation_SetDataObject`) can be classified and timed; a DataObject pointer or identity is not extracted from that manifest.
- `--providers` reports the three providers in the CLI inventory. It is not a complete inventory of every provider that capture may enable.
- A registered or enabled provider is not proof that it emitted events. Validate event production using an ETL captured on the target Windows system.
- ETL processing uses Windows ETW and TDH APIs. Build and end-to-end trace validation must be performed on Windows.
- ComTrace is a static trace analysis tool. Its output does not establish that an operation is malicious or vulnerable.
