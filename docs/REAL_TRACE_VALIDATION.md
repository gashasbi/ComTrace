# Real ETL Validation

This validation runs on a Windows machine and uses the installed COM Performance and OLE Performance providers. New captures also attempt to enable COM-RundownInstrumentation and record its event count and available OID/IID/IPID observations separately. Capture also attempts COMRuntime PrematureStubRundown, informational RPC call, and RPCSS interface registration telemetry; their event counts are recorded independently. A new capture generates COM activation traffic and runs an OLE fixture that exercises `IDataObject::GetData` and drop-target registration/revocation. In interactive mode, the fixture also calls the OLE clipboard APIs and `DoDragDrop`, then restores the previous clipboard data object. It checks the resulting ETL and verifies that converting it to CTRA preserves the event stream and the derived COM/OLE analysis.

## Capture and validate

Open **64-bit PowerShell as Administrator** in the repository directory after building Release:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\validate-real-trace.ps1 -ComTraceExe .\out\build\windows-vs2022-x64\Release\ComTrace.exe -ComTraceArchiveExe .\out\build\windows-vs2022-x64\Release\ComTraceArchive.exe
```

The script captures a 30-second ETL by default. During capture it activates several built-in Windows COM components, including Shell.Application, Scripting.Dictionary, and WScript.Shell. It also runs `ComTraceOleActivityFixture.exe`, which creates a test `IDataObject`, calls `IDataObject::GetData`, creates a hidden window, and registers and revokes an OLE drop target. The fixture deliberately avoids the system clipboard and `DoDragDrop`, because those interactive desktop operations can block on headless CI runners. Increase the duration with `-CaptureSeconds 60` when you also need time to reproduce an application-specific operation.

The fixture executable is built with the project and is included in the Windows release package. The script requires successful COM activation. A normal validation run also requires at least one matched OLE clipboard or drag-and-drop operation in the trace. `-AllowMissingOleEvents` is intended only for headless CI images where the OLE provider emits no clipboard or drag-and-drop lifecycle events; it records `NOT_EMITTED` and keeps the OLE unit tests active. Do not use this switch when validating an application trace on a target machine. To validate OLE lifecycle events, capture on an interactive Windows desktop and reproduce clipboard or drag-and-drop activity during the capture window.

When using the Windows ZIP release, extract the archive and run the same command with the executables in the extracted directory:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\validate-real-trace.ps1 -ComTraceExe .\ComTrace.exe -ComTraceArchiveExe .\ComTraceArchive.exe
```


## Interactive OLE lifecycle validation

The default automated fixture is headless-safe and does not use the system clipboard or execute `DoDragDrop`. Interactive mode enables those additional API calls and restores the previous clipboard data object before prompting for real desktop operations. Use the interactive mode below on a logged-in Windows desktop to validate those provider events.

Open **64-bit PowerShell as Administrator** in the repository directory. Build Release first, then run:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\validate-real-trace.ps1 -ComTraceExe .\out\build\windows-vs2022-x64\Release\ComTrace.exe -ComTraceArchiveExe .\out\build\windows-vs2022-x64\Release\ComTraceArchive.exe -CaptureSeconds 180 -InteractiveOleValidation
```

When the script prompts, use a real application to copy and paste content, then perform a real drag-and-drop operation. Return to PowerShell and press Enter before the 180-second capture ends. Do not pass `-AllowMissingOleEvents`; interactive validation must fail if the required OLE lifecycle events and matched start/stop operations are absent. The JSON summary records the Windows edition, version, build, OS architecture, PowerShell version, process architecture, and whether the shell was elevated.

If the desktop is remote, make sure the interactive session remains logged in and unlocked while the test runs. Clipboard redirection from Remote Desktop can change which machine owns the clipboard; perform the operation inside the Windows session being traced.

## Platform and architecture matrix

The supported build and release target is Windows x64. The regular GitHub Actions workflow currently validates Release and Debug configurations on a Windows Server 2022 x64 runner. This is useful Windows CI coverage, but it is not a substitute for testing on client editions of Windows.

Run the following manual matrix before a production release and attach each validation JSON summary to the release checklist:

| Environment | Build | Automated tests | Real COM capture | Interactive OLE |
|---|---|---|---|---|
| Windows 10 x64 | Release and Debug | Required | Required | Required |
| Windows 11 x64 | Release and Debug | Required | Required | Required |
| Windows Server 2022 x64 | Release and Debug | Required (CI) | Required (CI) | Required on an interactive server session |
| x86 or ARM64 | Not a supported release target | Not currently validated | Not currently validated | Not currently validated |

For each client OS, record the exact Windows build from the validation JSON. If a required OS image or interactive desktop is unavailable, leave that matrix cell unverified rather than treating a different Windows runner as equivalent. The x86 and ARM64 targets remain outside the current supported release scope; do not advertise them until dedicated configure, build, test, and real-trace runs pass.

## Repeated-run and resource checks

After one successful interactive trace validation, repeat capture and archive conversion several times on each target Windows build. Use a new output directory for each run so the ETL, CTRA, and reports are retained:

```powershell
1..5 | ForEach-Object {
    $output = ".\validation-run-$_"
    pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\validate-real-trace.ps1 -ComTraceExe .\out\build\windows-vs2022-x64\Release\ComTrace.exe -ComTraceArchiveExe .\out\build\windows-vs2022-x64\Release\ComTraceArchive.exe -CaptureSeconds 30 -OutputDirectory $output
    if ($LASTEXITCODE -ne 0) { throw "Validation run $_ failed." }
}
```

Check that each run exits normally, the ETW session is stopped, all archive checksums pass, and no temporary files or trace sessions accumulate. For native heap and handle leak investigation, run the Release executables under Windows Application Verifier or an equivalent native diagnostic tool; CTest failure-path coverage and repeated successful runs are useful regression checks, but they are not a proof that no leak exists.

## Validate an existing ETL

An existing ETL can be checked without starting an ETW session or running as Administrator:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File .\scripts\validate-real-trace.ps1 -ComTraceExe .\out\build\windows-vs2022-x64\Release\ComTrace.exe -ComTraceArchiveExe .\out\build\windows-vs2022-x64\Release\ComTraceArchive.exe -EtlPath C:\Traces\application.etl
```

Use a trace that contains COM Performance and OLE Performance events, including OLE clipboard or drag-and-drop operations with at least one matched start/stop pair. A trace without either type of activity will fail validation.

## What the script verifies

1. The ETL opens and can be processed by `ComTrace.exe --inspect` without event-processing failures.
2. `ComTrace.exe --search <trace.etl> --com` finds COM Performance events, reports matched COM calls by `CallTraceId`, `ActivityId`, and the weaker process/thread/operation fallback, and counts events missing each identifier.
3. `ComTrace.exe --search <trace.etl> --ole` finds OLE Performance events and validates clipboard or drag-and-drop activity, OLE start/stop pairing, and missing ActivityId counts.
4. `ComTrace.exe --search <trace.etl> --provider {2957313D-FCAA-5D4A-2F69-32CE5F0AC44E}` records COM-RundownInstrumentation events and available OID/IID/IPID fields. No events is reported as `NOT_EMITTED` and does not fail the validation.
5. `ComTrace.exe --search <trace.etl> --provider {BF406804-6AFA-46E7-8A48-6C357E1D6D61}`, `{6AD52B32-D609-4BE9-AE07-CE8DAE937E39}`, and `{D8975F88-7DDB-4ED0-91BF-3ADF48C48E0C}` records COMRuntime stub-rundown, RPC call, and RPCSS interface-registration events independently. Their counts may be zero and do not fail validation.
6. `ComTraceArchive.exe` converts the ETL to a CTRA archive.
7. `ComTraceArchive.exe --inspect` validates the archive and reports a valid CTRA v2 CRC32 checksum.
8. `ComTrace.exe --compare <trace.etl> <trace.ctra> --json` reports identical event totals and event-type totals, no event-type or COM-object differences, and matching COM/OLE timeline summaries and per-operation timings.

The RPC search also classifies manifest-defined FunctionTrace start/stop records separately from RPC client/server calls. These events describe RPC function tracing metadata, not COM object lifetime. The COM Performance provider can encode `CallTraceId` as either `UInt32` or `UInt64`. ComTrace accepts both scalar forms for that property while keeping its generic typed property readers strict. `CallTraceId` remains the primary match key. When both COM events lack it, matching non-empty `ActivityId`, process ID, thread ID, and operation are used as the second level. When both events lack both identifiers, the same process ID, thread ID, and operation can be used as a weaker third level; the report keeps that count separate so it is not confused with identifier-based matching. Nested starts use last-in-first-out pairing. One-sided or mismatched identifiers are never treated as matches.

OLE operation pairs report whether they matched through primary ActivityId, RelatedActivityId, or the weaker process ID, thread ID, and operation fallback. RelatedActivityId is used only when both events lack primary ActivityId and expose the same RelatedActivityId. The process/thread fallback is used only when both events lack both activity identifiers; one-sided or conflicting identifiers are never paired. Reversed start/stop timestamp order is retained as an invalid duration rather than silently omitted, and the inspection output and validation summary report its count.

The OLE Performance provider manifest inspected on Windows Server 2022 defines events but provides no payload templates or data fields. ComTrace also calls `TdhGetEventInformation` for displayed events and prints any event properties that TDH exposes at runtime. The validation summary records the number of OLE payload properties and how many property names contain `DataObject`. The raw OLE search report preserves the property names and decoded values for inspection. A DataObject pointer or identity is not inferred from raw payload bytes; it will only be added to identity analysis when the runtime schema exposes a stable, documented property and its meaning can be verified.

A successful run prints `Real ETL validation: PASS` and writes a JSON validation summary. The ETL, CTRA archive, and text/JSON reports are preserved in the output directory. By default this directory is `comtrace-validation` under the current directory; set `-OutputDirectory` to choose another location.

The number of CLSID, IID, IPID, or OID values reported is included for review, but is not a pass/fail requirement. COM-RundownInstrumentation event and identity counts are also recorded separately; absence of rundown events is not treated as a validation failure. COMRuntime, RPC, and RPCSS provider totals and classified event counts are also recorded separately; missing events from these optional providers are not treated as validation failures. A specific provider event may not expose those fields in its installed TDH schema. ComTrace does not infer missing identities from arbitrary payload bytes.

## Target application validation

The built-in activations and default OLE interface fixture provide repeatable COM traffic and headless-safe OLE interface coverage. The default fixture does not generate a system clipboard operation or a complete drag-and-drop operation; interactive mode additionally exercises the OLE clipboard APIs and `DoDragDrop`. To validate a particular application's OLE behavior, start the validation script on an interactive Windows desktop and reproduce the target clipboard or drag-and-drop operation during the capture window. For an already captured trace, pass its path with `-EtlPath`; that ETL must already contain both COM and OLE lifecycle activity.

The default fixture validates `IDataObject` data retrieval and OLE drop-target registration/revocation without using the system clipboard. Interactive mode additionally exercises `OleSetClipboard`, `OleGetClipboard`, `OleFlushClipboard`, and a bounded `DoDragDrop` call. The manually reproduced application operations remain necessary to validate the target application's desktop behavior. The automated test does not claim that every COM/OLE provider event or every identity field is present on every Windows build. Provider registration, event emission, and payload schemas vary by OS build and application.
