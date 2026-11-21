# Real ETL Validation

This validation runs on a Windows machine and uses the installed COM Performance and OLE Performance providers. A new capture generates both COM activation traffic and real OLE clipboard activity through `ComTraceOleActivityFixture.exe`. It checks the resulting ETL and verifies that converting it to CTRA preserves the event stream and the derived COM/OLE analysis.

## Capture and validate

Open **64-bit PowerShell as Administrator** in the repository directory after building Release:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\validate-real-trace.ps1 -ComTraceExe .\out\build\windows-vs2022-x64\Release\ComTrace.exe -ComTraceArchiveExe .\out\build\windows-vs2022-x64\Release\ComTraceArchive.exe
```

The script captures a 30-second ETL by default. During capture it activates several built-in Windows COM components, including Shell.Application, Scripting.Dictionary, and WScript.Shell. It also runs `ComTraceOleActivityFixture.exe`, which publishes a test `IDataObject` to the OLE clipboard, reads it back, checks the current clipboard object, and flushes it. Increase the duration with `-CaptureSeconds 60` when you also need time to reproduce an application-specific operation.

The fixture executable is built with the project and is included in the Windows release package. The script requires successful COM activation and OLE clipboard activity, plus events classified by both the COM Performance and OLE Performance providers. If the installed Windows image does not provide the required COM/OLE services, use an existing ETL captured on a machine where both types of activity occurred.

When using the Windows ZIP release, extract the archive and run the same command with the executables in the extracted directory:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\validate-real-trace.ps1 -ComTraceExe .\ComTrace.exe -ComTraceArchiveExe .\ComTraceArchive.exe
```


## Validate an existing ETL

An existing ETL can be checked without starting an ETW session or running as Administrator:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\validate-real-trace.ps1 -ComTraceExe .\out\build\windows-vs2022-x64\Release\ComTrace.exe -ComTraceArchiveExe .\out\build\windows-vs2022-x64\Release\ComTraceArchive.exe -EtlPath C:\Traces\application.etl
```

Use a trace that contains COM Performance and OLE Performance events, including OLE clipboard operations with at least one matched start/stop pair. A trace without either type of activity will fail validation.

## What the script verifies

1. The ETL opens and can be processed by `ComTrace.exe --inspect` without event-processing failures.
2. `ComTrace.exe --search <trace.etl> --com` finds COM Performance events, reports matched COM calls by `CallTraceId` and `ActivityId`, and counts events missing each identifier.
3. `ComTrace.exe --search <trace.etl> --ole` finds OLE Performance events and validates clipboard activity, OLE start/stop pairing, and missing ActivityId counts.
4. `ComTraceArchive.exe` converts the ETL to a CTRA archive.
5. `ComTraceArchive.exe --inspect` validates the archive and reports a valid CTRA v2 CRC32 checksum.
6. `ComTrace.exe --compare <trace.etl> <trace.ctra> --json` reports identical event totals and event-type totals, no event-type or COM-object differences, and matching COM/OLE timeline summaries and per-operation timings.

The COM Performance provider can encode `CallTraceId` as either `UInt32` or `UInt64`. ComTrace accepts both scalar forms for that property while keeping its generic typed property readers strict. `CallTraceId` remains the primary match key. When both COM events lack it, ComTrace can pair them only when their non-empty `ActivityId`, process ID, thread ID, and operation all match. Nested starts use last-in-first-out pairing. A one-sided `CallTraceId`, missing `ActivityId`, or mismatched key is never treated as a match.

OLE operation pairs report whether they matched through ActivityId or through the weaker process ID, thread ID, and operation fallback. The fallback is used only when both events lack ActivityId; mismatched or one-sided Activity IDs are not paired. This distinction is included in the inspection and comparison reports.

A successful run prints `Real ETL validation: PASS` and writes a JSON validation summary. The ETL, CTRA archive, and text/JSON reports are preserved in the output directory. By default this directory is `comtrace-validation` under the current directory; set `-OutputDirectory` to choose another location.

The number of CLSID, IID, IPID, or OID values reported is included for review, but is not a pass/fail requirement. A specific provider event may not expose those fields in its installed TDH schema. ComTrace does not infer missing identities from arbitrary payload bytes.

## Target application validation

The built-in activations and OLE clipboard fixture provide repeatable COM/OLE traffic. To validate a particular application's behavior, start the validation script and reproduce the target operation during the capture window. For an already captured trace, pass its path with `-EtlPath`; that ETL must already contain both COM and OLE activity.

The automated test does not claim that every COM/OLE provider event or every identity field is present on every Windows build. Provider registration, event emission, and payload schemas vary by OS build and application.
