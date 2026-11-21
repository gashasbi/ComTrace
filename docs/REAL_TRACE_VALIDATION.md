# Real ETL Validation

This validation runs on a Windows machine and uses the installed COM Performance provider. It checks a real ETL trace and then verifies that converting it to CTRA preserves the event stream and the derived COM/OLE analysis.

## Capture and validate

Open **64-bit PowerShell as Administrator** in the repository directory after building Release:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\validate-real-trace.ps1 -ComTraceExe .\out\build\windows-vs2022-x64\Release\ComTrace.exe -ComTraceArchiveExe .\out\build\windows-vs2022-x64\Release\ComTraceArchive.exe
```

The script captures a 30-second ETL by default. During capture it activates several built-in Windows COM components, including Shell.Application, Scripting.Dictionary, and WScript.Shell. Increase the duration with `-CaptureSeconds 60` when you also need time to reproduce an application-specific operation.

The script requires at least one successful COM activation and at least one event classified as a COM Performance event. If the installed Windows image does not provide any of the built-in COM classes, use an existing ETL from a machine where the intended COM activity was captured.

## Validate an existing ETL

An existing ETL can be checked without starting an ETW session or running as Administrator:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\validate-real-trace.ps1 -ComTraceExe .\out\build\windows-vs2022-x64\Release\ComTrace.exe -ComTraceArchiveExe .\out\build\windows-vs2022-x64\Release\ComTraceArchive.exe -EtlPath C:\Traces\application.etl
```

Use a trace that contains COM Performance events. A trace from a different provider or a capture in which no COM activity occurred will fail the COM-event check.

## What the script verifies

1. The ETL opens and can be processed by `ComTrace.exe --inspect` without event-processing failures.
2. `ComTrace.exe --search <trace.etl> --com` finds at least one COM Performance event.
3. `ComTraceArchive.exe` converts the ETL to a CTRA archive.
4. `ComTraceArchive.exe --inspect` validates the archive and reports a valid CTRA v2 CRC32 checksum.
5. `ComTrace.exe --compare <trace.etl> <trace.ctra> --json` reports identical event totals and event-type totals, no event-type or COM-object differences, and matching COM/OLE timeline summaries and per-operation timings.
6. The COM search reports at least one matched start/stop pair. The validation summary records matched operations and events without a usable `CallTraceId`.

The COM Performance provider can encode `CallTraceId` as either `UInt32` or `UInt64`. ComTrace accepts both documented scalar forms for that property while keeping its generic typed property readers strict. Events without a `CallTraceId` remain unmatched rather than being paired by guesswork.

A successful run prints `Real ETL validation: PASS` and writes a JSON validation summary. The ETL, CTRA archive, and text/JSON reports are preserved in the output directory. By default this directory is `comtrace-validation` under the current directory; set `-OutputDirectory` to choose another location.

The number of CLSID, IID, IPID, or OID values reported is included for review, but is not a pass/fail requirement. A specific provider event may not expose those fields in its installed TDH schema. ComTrace does not infer missing identities from arbitrary payload bytes.

## Target application validation

The built-in activations provide repeatable COM traffic. To validate a particular application's behavior, start the validation script and reproduce the target operation during the capture window. For an already captured application trace, pass its path with `-EtlPath`.

The automated test does not claim that every COM/OLE provider event or every identity field is present on every Windows build. Provider registration, event emission, and payload schemas vary by OS build and application.
