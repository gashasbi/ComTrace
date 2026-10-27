# ComTrace

ComTrace is a small native Windows command-line tool for recording and inspecting ETW trace files related to COM and OLE activity.

## Requirements

- Windows
- Visual Studio with the C/C++ workload
- CMake 4.2 or newer, matching `CMakePresets.json`

## Build and test

Run these commands from a Developer PowerShell in the project directory:

```powershell
cmake --preset windows-msvc-x64
cmake --build --preset windows-msvc-x64-debug
ctest --preset windows-msvc-x64-debug
```

For a Release build, use `windows-msvc-x64-release` instead of the Debug preset.

## Commands

```text
ComTrace --help
ComTrace --version
ComTrace --self-test
ComTrace --providers
ComTrace --capture <seconds> <output.etl>
ComTrace --inspect <file.etl> [max-events]
```

Capture duration must be between 1 and 3600 seconds. The output path must end in `.etl` and must not already exist. Capture may require an elevated terminal depending on the machine's ETW permissions.

Inspection prints a bounded sample of events, a summary of event types, and supported payload property values. The default sample limit is 100; the accepted range is 1 to 10000. Some property types or schemas are intentionally skipped when they are unsupported or exceed the tool's size limits.

## Current coverage and limitations

- Capture enables `Microsoft-Windows-COM-Perf` and `Microsoft-Windows-OLE-Perf`.
- The event classifiers recognize the mappings explicitly defined in `src/com_event.c` and `src/ole_event.c`. Unknown provider IDs, event IDs, versions, or opcodes are not classified.
- `--providers` reports the three providers currently listed in the CLI catalog. This list is not a complete inventory of every provider that capture may enable.
- A provider being registered or enabled does not prove it emitted events. Validate event production from an actual ETL trace on the target Windows system.
- ComTrace is a static trace inspection tool. It does not establish that a COM/OLE operation is malicious or vulnerable.
- ETL processing uses Windows ETW and TDH APIs, so a full build and end-to-end trace test must be performed on Windows.
