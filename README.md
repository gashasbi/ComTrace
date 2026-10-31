# ComTrace

ComTrace is a small native Windows command-line tool for recording and inspecting ETW trace files related to COM and OLE activity.

---

## Requirements

- Windows 10/11 x64
- Visual Studio 2022 with **Desktop development with C++** and the Windows 10/11 SDK
- CMake 3.21 or newer
- Ninja only for the VS Code/Ninja workflow

## Visual Studio 2022

Open **x64 Native Tools Command Prompt for VS 2022** in the project folder and run:

```cmd
cmake --preset windows-vs2022-x64
cmake --build --preset windows-vs2022-x64-debug
ctest --preset windows-vs2022-x64-debug
```

For Release, replace `debug` with `release` in the build and test preset names. Open `out\build\windows-vs2022-x64\ComTrace.sln` to work in Visual Studio.

## VS Code / Ninja

Open **x64 Native Tools Command Prompt for VS 2022** in the project folder, then run:

```cmd
cmake --preset windows-msvc-x64-debug
cmake --build --preset windows-msvc-x64-debug
ctest --preset windows-msvc-x64-debug
```

For Release, replace `debug` with `release` in all three preset names. In VS Code, install the recommended **CMake Tools** and **C/C++** extensions and select the corresponding configure preset.

---

## Run

```cmd
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