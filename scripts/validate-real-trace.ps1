param(
    [Parameter(Mandatory = $true)]
    [string]$ComTraceExe,

    [Parameter(Mandatory = $true)]
    [string]$ComTraceArchiveExe,

    [string]$ComTraceOleActivityFixtureExe,

    [string]$EtlPath,

    [string]$OutputDirectory = (Join-Path (Get-Location).Path "comtrace-validation"),

    [ValidateRange(10, 3600)]
    [int]$CaptureSeconds = 30,

    [switch]$AllowMissingOleEvents
)

$ErrorActionPreference = "Stop"

function Invoke-NativeCommand {
    param(
        [string]$Executable,
        [string[]]$Arguments,
        [string]$Description
    )

    $output = & $Executable @Arguments 2>&1
    $exitCode = $LASTEXITCODE
    $text = ($output | Out-String).TrimEnd()

    if ($exitCode -ne 0) {
        throw "$Description failed with exit code $exitCode.$([Environment]::NewLine)$text"
    }

    return $text
}

function Release-ComObject {
    param([object]$Value)

    if ($null -ne $Value -and [Runtime.InteropServices.Marshal]::IsComObject($Value)) {
        [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($Value)
    }
}

function Invoke-ComActivation {
    param(
        [string]$ProgId,
        [scriptblock]$Action,
        [System.Collections.Generic.List[string]]$Failures
    )

    $instance = $null

    try {
        $instance = New-Object -ComObject $ProgId
        & $Action $instance
        return $true
    }
    catch {
        [void]$Failures.Add("$ProgId : $($_.Exception.Message)")
        return $false
    }
    finally {
        Release-ComObject $instance
    }
}

if (-not [Environment]::Is64BitProcess) {
    throw "Run this script from 64-bit PowerShell."
}

if (-not (Test-Path -LiteralPath $ComTraceExe -PathType Leaf)) {
    throw "ComTrace.exe was not found: $ComTraceExe"
}

if (-not (Test-Path -LiteralPath $ComTraceArchiveExe -PathType Leaf)) {
    throw "ComTraceArchive.exe was not found: $ComTraceArchiveExe"
}

$ComTraceExe = (Resolve-Path -LiteralPath $ComTraceExe).Path
$ComTraceArchiveExe = (Resolve-Path -LiteralPath $ComTraceArchiveExe).Path
$captureNewTrace = [string]::IsNullOrWhiteSpace($EtlPath)

if ([string]::IsNullOrWhiteSpace($ComTraceOleActivityFixtureExe)) {
    $ComTraceOleActivityFixtureExe = Join-Path (Split-Path -Parent $ComTraceExe) "ComTraceOleActivityFixture.exe"
}

if ($captureNewTrace) {
    if (-not (Test-Path -LiteralPath $ComTraceOleActivityFixtureExe -PathType Leaf)) {
        throw "ComTraceOleActivityFixture.exe was not found: $ComTraceOleActivityFixtureExe"
    }

    $ComTraceOleActivityFixtureExe = (Resolve-Path -LiteralPath $ComTraceOleActivityFixtureExe).Path
}

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$OutputDirectory = (Resolve-Path -LiteralPath $OutputDirectory).Path

$runId = Get-Date -Format "yyyyMMdd-HHmmss"
$archivePath = Join-Path $OutputDirectory "comtrace-$runId.ctra"
$inspectPath = Join-Path $OutputDirectory "comtrace-$runId-inspect.txt"
$searchPath = Join-Path $OutputDirectory "comtrace-$runId-com-search.txt"
$oleSearchPath = Join-Path $OutputDirectory "comtrace-$runId-ole-search.txt"
$rundownSearchPath = Join-Path $OutputDirectory "comtrace-$runId-com-rundown-search.txt"
$comRuntimeSearchPath = Join-Path $OutputDirectory "comtrace-$runId-com-runtime-search.txt"
$rpcSearchPath = Join-Path $OutputDirectory "comtrace-$runId-rpc-search.txt"
$rpcssSearchPath = Join-Path $OutputDirectory "comtrace-$runId-rpcss-search.txt"
$oleFixturePath = Join-Path $OutputDirectory "comtrace-$runId-ole-fixture.txt"
$archiveInspectPath = Join-Path $OutputDirectory "comtrace-$runId-archive-inspect.txt"
$comparePath = Join-Path $OutputDirectory "comtrace-$runId-compare.json"
$summaryPath = Join-Path $OutputDirectory "comtrace-$runId-validation.json"

$activationCount = 0
$activationFailures = New-Object 'System.Collections.Generic.List[string]'

if ([string]::IsNullOrWhiteSpace($EtlPath)) {
    $identity = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)

    if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
        throw "ETW capture requires an elevated PowerShell. Re-run this script as Administrator."
    }

    $EtlPath = Join-Path $OutputDirectory "comtrace-$runId-real.etl"
    $captureOutputPath = Join-Path $OutputDirectory "comtrace-$runId-capture.txt"
    $captureErrorPath = Join-Path $OutputDirectory "comtrace-$runId-capture-error.txt"
    $captureArguments = '--capture {0} "{1}"' -f $CaptureSeconds, $EtlPath

    $captureProcess = Start-Process -FilePath $ComTraceExe -ArgumentList $captureArguments -PassThru -RedirectStandardOutput $captureOutputPath -RedirectStandardError $captureErrorPath

    $captureStarted = $false
    $startupDeadline = (Get-Date).AddSeconds(20)

    while ((Get-Date) -lt $startupDeadline -and -not $captureProcess.HasExited) {
        if (Test-Path -LiteralPath $captureOutputPath) {
            $captureText = Get-Content -LiteralPath $captureOutputPath -Raw
            if ($captureText -match "ETW capture started") {
                $captureStarted = $true
                break
            }
        }

        Start-Sleep -Milliseconds 200
    }

    if (-not $captureStarted) {
        if (-not $captureProcess.HasExited) {
            [void]$captureProcess.WaitForExit(5000)
        }

        $captureText = ""
        $captureErrorText = ""

        if (Test-Path -LiteralPath $captureOutputPath) {
            $captureText = Get-Content -LiteralPath $captureOutputPath -Raw
        }
        if (Test-Path -LiteralPath $captureErrorPath) {
            $captureErrorText = Get-Content -LiteralPath $captureErrorPath -Raw
        }

        throw "ComTrace could not start ETW capture.$([Environment]::NewLine)$captureText$([Environment]::NewLine)$captureErrorText"
    }

    Write-Host "ETW capture is running. Generating COM activity."

    for ($index = 1; $index -le 5; $index++) {
        $value = "ComTrace-$index"

        if (Invoke-ComActivation -ProgId "Shell.Application" -Failures $activationFailures -Action {
                param($com)
                $folder = $com.NameSpace($env:TEMP)
                if ($null -eq $folder) {
                    throw "Shell.Application did not return the temporary folder."
                }
                Release-ComObject $folder
            }) {
            $activationCount++
        }

        if (Invoke-ComActivation -ProgId "Scripting.Dictionary" -Failures $activationFailures -Action {
                param($com)
                $com.Add($value, $index)
                if ([int]$com.Item($value) -ne $index) {
                    throw "Scripting.Dictionary returned an unexpected value."
                }
            }) {
            $activationCount++
        }

        if (Invoke-ComActivation -ProgId "WScript.Shell" -Failures $activationFailures -Action {
                param($com)
                $expanded = $com.ExpandEnvironmentStrings("%TEMP%")
                if ([string]::IsNullOrWhiteSpace($expanded)) {
                    throw "WScript.Shell returned an empty environment variable."
                }
            }) {
            $activationCount++
        }
    }

    Write-Host "Generating OLE clipboard and drag-drop registration activity."
    $oleFixtureOutput = Invoke-NativeCommand -Executable $ComTraceOleActivityFixtureExe -Arguments @() -Description "OLE clipboard activity fixture"
    Set-Content -LiteralPath $oleFixturePath -Value $oleFixtureOutput -Encoding UTF8

    if ($oleFixtureOutput -notmatch "OLE clipboard activity succeeded") {
        throw "The OLE activity fixture did not report success. Review $oleFixturePath"
    }

    if (-not $captureProcess.WaitForExit(($CaptureSeconds + 30) * 1000)) {
        throw "ETW capture did not finish within the expected time. Process ID: $($captureProcess.Id)"
    }

    if ($captureProcess.ExitCode -ne 0) {
        $captureText = Get-Content -LiteralPath $captureOutputPath -Raw
        $captureErrorText = Get-Content -LiteralPath $captureErrorPath -Raw
        throw "ETW capture failed with exit code $($captureProcess.ExitCode).$([Environment]::NewLine)$captureText$([Environment]::NewLine)$captureErrorText"
    }

    if ($activationCount -eq 0) {
        throw "None of the built-in COM activation tests succeeded. ETL was preserved at $EtlPath"
    }
}
else {
    if (-not (Test-Path -LiteralPath $EtlPath -PathType Leaf)) {
        throw "ETL input was not found: $EtlPath"
    }

    $EtlPath = (Resolve-Path -LiteralPath $EtlPath).Path
}

if (-not (Test-Path -LiteralPath $EtlPath -PathType Leaf)) {
    throw "ETL capture did not create an input file: $EtlPath"
}

Write-Host "Inspecting ETL."
$inspectOutput = Invoke-NativeCommand -Executable $ComTraceExe -Arguments @("--inspect", $EtlPath, "10000") -Description "ETL inspection"
Set-Content -LiteralPath $inspectPath -Value $inspectOutput -Encoding UTF8

$processProviderGuid = "{3D6FA8D0-FE05-11D0-9DDA-00C04FD7BA7C}"
$processStartPattern = "(?im)^\s*$([regex]::Escape($processProviderGuid))\s*\|\s*\d+\s*\|\s*\d+\s*\|\s*\d+\s*\|\s*\d+\s*\|\s*1\s*\|"
$processEndPattern = "(?im)^\s*$([regex]::Escape($processProviderGuid))\s*\|\s*\d+\s*\|\s*\d+\s*\|\s*\d+\s*\|\s*\d+\s*\|\s*2\s*\|"
$processStartTypeCount = [regex]::Matches($inspectOutput, $processStartPattern).Count
$processEndTypeCount = [regex]::Matches($inspectOutput, $processEndPattern).Count
$processLifecycleMatch = [regex]::Match(
    $inspectOutput,
    "Process lifecycle events:\s*start=(\d+) end=(\d+) dc_start=(\d+) dc_end=(\d+) defunct=(\d+) decode_failures=(\d+)"
)

if (-not $processLifecycleMatch.Success) {
    throw "The process lifecycle summary was not found. Review $inspectPath"
}

$processStartCount = [UInt64]::Parse($processLifecycleMatch.Groups[1].Value)
$processEndCount = [UInt64]::Parse($processLifecycleMatch.Groups[2].Value)
$processDecodeFailures = [UInt64]::Parse($processLifecycleMatch.Groups[6].Value)

if ($processStartTypeCount -eq 0 -or $processEndTypeCount -eq 0 -or
    $processStartCount -eq 0 -or $processEndCount -eq 0 -or
    $processDecodeFailures -ne 0) {
    throw "Kernel Process start/stop payloads were not both decoded. Provider Start=$processStartTypeCount End=$processEndTypeCount; decoded Start=$processStartCount End=$processEndCount. Review $inspectPath"
}

$processGraphMatch = [regex]::Match(
    $inspectOutput,
    "Process node generations:\s*known=(\d+)\s+unknown=(\d+)"
)
if (-not $processGraphMatch.Success) {
    throw "The relationship graph did not report Process generation coverage. Review $inspectPath"
}

$processGraphKnownGenerations = [UInt64]::Parse($processGraphMatch.Groups[1].Value)
$processGraphUnknownGenerations = [UInt64]::Parse($processGraphMatch.Groups[2].Value)
if ($processGraphKnownGenerations -eq 0) {
    throw "No relationship graph Process node was matched to a lifecycle generation. Review $inspectPath"
}

Write-Host "Checking for COM Performance events."
$searchOutput = Invoke-NativeCommand -Executable $ComTraceExe -Arguments @("--search", $EtlPath, "--com", "--max-events", "10000") -Description "COM event search"
Set-Content -LiteralPath $searchPath -Value $searchOutput -Encoding UTF8

$match = [regex]::Match($searchOutput, "Events matching filters:\s*(\d+)")
if (-not $match.Success) {
    throw "The COM search output did not contain an event count. Review $searchPath"
}

$comEventCount = [UInt64]::Parse($match.Groups[1].Value)
if ($comEventCount -eq 0) {
    throw "No COM Performance events were found in the ETL. Review the capture conditions and $searchPath"
}

$identityPattern = "(?im)(?:com_)?(?:clsid|iid|ipid|oid)="
$identityValueCount = [regex]::Matches($searchOutput, $identityPattern).Count

$matchedTimeline = [regex]::Match($searchOutput, "Matched start/stop:\s*(\d+)")
$matchedByCallTrace = [regex]::Match($searchOutput, "Matched by CallTraceId:\s*(\d+)")
$matchedByActivity = [regex]::Match($searchOutput, "Matched by ActivityId:\s*(\d+)")
$matchedByProcessThread = [regex]::Match($searchOutput, "Matched by process/thread/operation:\s*(\d+)")
$missingCallTrace = [regex]::Match($searchOutput, "Missing CallTraceId:\s*(\d+)")
$missingActivity = [regex]::Match($searchOutput, "Missing ActivityId:\s*(\d+)")
if (-not $matchedTimeline.Success -or
    -not $matchedByCallTrace.Success -or
    -not $matchedByActivity.Success -or
    -not $matchedByProcessThread.Success -or
    -not $missingCallTrace.Success -or
    -not $missingActivity.Success) {
    throw "The COM timeline summary did not contain its pairing counters. Review $searchPath"
}

$comTimelineMatched = [UInt64]::Parse($matchedTimeline.Groups[1].Value)
$comTimelineMatchedByCallTrace = [UInt64]::Parse($matchedByCallTrace.Groups[1].Value)
$comTimelineMatchedByActivity = [UInt64]::Parse($matchedByActivity.Groups[1].Value)
$comTimelineMatchedByProcessThread = [UInt64]::Parse($matchedByProcessThread.Groups[1].Value)
$comTimelineMissingCallTrace = [UInt64]::Parse($missingCallTrace.Groups[1].Value)
$comTimelineMissingActivity = [UInt64]::Parse($missingActivity.Groups[1].Value)
if ($comTimelineMatched -eq 0) {
    throw "No COM start/stop pairs were matched. Review CallTraceId, ActivityId, and process/thread/operation data in $searchPath"
}

$comMatchTotal = $comTimelineMatchedByCallTrace +
                 $comTimelineMatchedByActivity +
                 $comTimelineMatchedByProcessThread
if ($comMatchTotal -ne $comTimelineMatched) {
    throw "COM match-source counts do not add up to the total matched operations."
}

Write-Host "Checking for OLE Performance events."
$oleSearchOutput = Invoke-NativeCommand -Executable $ComTraceExe -Arguments @("--search", $EtlPath, "--ole", "--max-events", "10000") -Description "OLE event search"
Set-Content -LiteralPath $oleSearchPath -Value $oleSearchOutput -Encoding UTF8

$oleEventMatch = [regex]::Match($oleSearchOutput, "Events matching filters:\s*(\d+)")
$oleSummaryMatch = [regex]::Match(
    $oleSearchOutput,
    "(?ms)OLE timeline summary.*?Clipboard operations:\s*(\d+).*?Drag and Drop operations:\s*(\d+).*?Matched start/stop:\s*(\d+).*?Matched by ActivityId:\s*(\d+).*?Matched by process/thread/operation:\s*(\d+).*?Missing ActivityId:\s*(\d+)"
)
if (-not $oleEventMatch.Success -or -not $oleSummaryMatch.Success) {
    throw "The OLE search output did not contain event and timeline counters. Review $oleSearchPath"
}

$oleEventCount = [UInt64]::Parse($oleEventMatch.Groups[1].Value)
$oleValidationStatus = "PASS"
$oleClipboardOperations = [UInt64]::Parse($oleSummaryMatch.Groups[1].Value)
$oleDragDropOperations = [UInt64]::Parse($oleSummaryMatch.Groups[2].Value)
$oleTimelineMatched = [UInt64]::Parse($oleSummaryMatch.Groups[3].Value)
$oleTimelineMatchedByActivity = [UInt64]::Parse($oleSummaryMatch.Groups[4].Value)
$oleTimelineMatchedByProcessThread = [UInt64]::Parse($oleSummaryMatch.Groups[5].Value)
$oleTimelineMissingActivity = [UInt64]::Parse($oleSummaryMatch.Groups[6].Value)
$olePayloadPropertyCount = [regex]::Matches(
    $oleSearchOutput,
    "(?im)^\s*property\[\d+\] name="
).Count
$oleDataObjectPropertyCount = [regex]::Matches(
    $oleSearchOutput,
    "(?im)^\s*property\[\d+\] name=[^\r\n]*DataObject"
).Count

if ($oleEventCount -eq 0 -and $AllowMissingOleEvents) {
    $oleValidationStatus = "NOT_EMITTED"
    Write-Warning "The OLE-Perf provider emitted no events on this Windows environment. OLE unit tests remain enabled."
} elseif ($oleEventCount -eq 0 -or
          ($oleClipboardOperations -eq 0 -and $oleDragDropOperations -eq 0) -or
          $oleTimelineMatched -eq 0) {
    throw "OLE validation requires at least one clipboard or drag-and-drop operation and a matched start/stop pair. Review $oleSearchPath"
}

$oleMatchTotal = $oleTimelineMatchedByActivity + $oleTimelineMatchedByProcessThread
if ($oleMatchTotal -ne $oleTimelineMatched) {
    throw "OLE match-source counts do not add up to the total matched operations."
}

Write-Host "Checking for COM rundown instrumentation events."
$rundownProviderGuid = "{2957313D-FCAA-5D4A-2F69-32CE5F0AC44E}"
$rundownSearchOutput = Invoke-NativeCommand -Executable $ComTraceExe -Arguments @("--search", $EtlPath, "--provider", $rundownProviderGuid, "--max-events", "10000") -Description "COM rundown event search"
Set-Content -LiteralPath $rundownSearchPath -Value $rundownSearchOutput -Encoding UTF8

$rundownEventMatch = [regex]::Match($rundownSearchOutput, "Events matching filters:\s*(\d+)")
if (-not $rundownEventMatch.Success) {
    throw "The COM rundown search output did not contain an event count. Review $rundownSearchPath"
}

$rundownEventCount = [UInt64]::Parse($rundownEventMatch.Groups[1].Value)
$rundownIdentityValueCount = [regex]::Matches(
    $rundownSearchOutput,
    "(?im)^\s*com_rundown_(?:oid|iid|ipid)="
).Count
$rundownValidationStatus = "PRESENT"
if ($rundownEventCount -eq 0) {
    $rundownValidationStatus = "NOT_EMITTED"
    Write-Warning "The COM-RundownInstrumentation provider emitted no events in this trace."
}

Write-Host "Checking for COMRuntime, RPC, and RPCSS events."
$comRuntimeProviderGuid = "{BF406804-6AFA-46E7-8A48-6C357E1D6D61}"
$rpcProviderGuid = "{6AD52B32-D609-4BE9-AE07-CE8DAE937E39}"
$rpcssProviderGuid = "{D8975F88-7DDB-4ED0-91BF-3ADF48C48E0C}"

$comRuntimeSearchOutput = Invoke-NativeCommand -Executable $ComTraceExe -Arguments @("--search", $EtlPath, "--provider", $comRuntimeProviderGuid, "--max-events", "10000") -Description "COMRuntime event search"
Set-Content -LiteralPath $comRuntimeSearchPath -Value $comRuntimeSearchOutput -Encoding UTF8
$comRuntimeEventMatch = [regex]::Match($comRuntimeSearchOutput, "Events matching filters:\s*(\d+)")
if (-not $comRuntimeEventMatch.Success) {
    throw "The COMRuntime search output did not contain an event count. Review $comRuntimeSearchPath"
}
$comRuntimeEventCount = [UInt64]::Parse($comRuntimeEventMatch.Groups[1].Value)
$comRuntimeStubRundownCount = [regex]::Matches(
    $comRuntimeSearchOutput,
    "(?im)^\s*com_runtime_event=PrematureStubRundown"
).Count

$rpcSearchOutput = Invoke-NativeCommand -Executable $ComTraceExe -Arguments @("--search", $EtlPath, "--provider", $rpcProviderGuid, "--max-events", "10000") -Description "RPC event search"
Set-Content -LiteralPath $rpcSearchPath -Value $rpcSearchOutput -Encoding UTF8
$rpcEventMatch = [regex]::Match($rpcSearchOutput, "Events matching filters:\s*(\d+)")
if (-not $rpcEventMatch.Success) {
    throw "The RPC search output did not contain an event count. Review $rpcSearchPath"
}
$rpcEventCount = [UInt64]::Parse($rpcEventMatch.Groups[1].Value)
$rpcClassifiedEventCount = [regex]::Matches(
    $rpcSearchOutput,
    "(?im)^\s*rpc_event="
).Count
$rpcClientCallStartCount = [regex]::Matches(
    $rpcSearchOutput,
    "(?im)^\s*rpc_event=RpcClientCall phase=start"
).Count
$rpcClientCallStopCount = [regex]::Matches(
    $rpcSearchOutput,
    "(?im)^\s*rpc_event=RpcClientCall phase=stop"
).Count
$rpcServerCallStartCount = [regex]::Matches(
    $rpcSearchOutput,
    "(?im)^\s*rpc_event=RpcServerCall phase=start"
).Count
$rpcServerCallStopCount = [regex]::Matches(
    $rpcSearchOutput,
    "(?im)^\s*rpc_event=RpcServerCall phase=stop"
).Count
$rpcFunctionTraceStartCount = [regex]::Matches(
    $rpcSearchOutput,
    "(?im)^\s*rpc_event=FunctionTrace phase=start"
).Count
$rpcFunctionTraceStopCount = [regex]::Matches(
    $rpcSearchOutput,
    "(?im)^\s*rpc_event=FunctionTrace phase=stop"
).Count
$rpcFunctionTraceEndpointCount = [regex]::Matches(
    $rpcSearchOutput,
    "(?im)^\s*rpc_event=FunctionTraceStart16 phase=start"
).Count
$rpcBreakdownTotal = $rpcClientCallStartCount +
                     $rpcClientCallStopCount +
                     $rpcServerCallStartCount +
                     $rpcServerCallStopCount +
                     $rpcFunctionTraceStartCount +
                     $rpcFunctionTraceStopCount +
                     $rpcFunctionTraceEndpointCount
if ($rpcBreakdownTotal -ne $rpcClassifiedEventCount -or
    $rpcClassifiedEventCount -gt $rpcEventCount) {
    throw "RPC classified event counts do not match the reported event breakdown."
}

$rpcssSearchOutput = Invoke-NativeCommand -Executable $ComTraceExe -Arguments @("--search", $EtlPath, "--provider", $rpcssProviderGuid, "--max-events", "10000") -Description "RPCSS event search"
Set-Content -LiteralPath $rpcssSearchPath -Value $rpcssSearchOutput -Encoding UTF8
$rpcssEventMatch = [regex]::Match($rpcssSearchOutput, "Events matching filters:\s*(\d+)")
if (-not $rpcssEventMatch.Success) {
    throw "The RPCSS search output did not contain an event count. Review $rpcssSearchPath"
}
$rpcssEventCount = [UInt64]::Parse($rpcssEventMatch.Groups[1].Value)
$rpcssClassifiedEventCount = [regex]::Matches(
    $rpcssSearchOutput,
    "(?im)^\s*rpcss_event="
).Count

Write-Host "Converting ETL to CTRA."
$archiveOutput = Invoke-NativeCommand -Executable $ComTraceArchiveExe -Arguments @($EtlPath, $archivePath) -Description "ETL to CTRA conversion"
if (-not (Test-Path -LiteralPath $archivePath -PathType Leaf)) {
    throw "CTRA conversion reported success but did not create $archivePath"
}

Write-Host "Validating CTRA checksum and records."
$archiveInspectOutput = Invoke-NativeCommand -Executable $ComTraceArchiveExe -Arguments @("--inspect", $archivePath) -Description "CTRA inspection"
Set-Content -LiteralPath $archiveInspectPath -Value $archiveInspectOutput -Encoding UTF8

if ($archiveInspectOutput -notmatch "Archive validation: PASS" -or
    $archiveInspectOutput -notmatch "Checksum: CRC32 valid") {
    throw "CTRA validation or CRC32 verification failed. Review $archiveInspectPath"
}

Write-Host "Comparing the original ETL with the CTRA round trip."
$compareOutput = Invoke-NativeCommand -Executable $ComTraceExe -Arguments @("--compare", $EtlPath, $archivePath, "--json") -Description "ETL and CTRA comparison"
Set-Content -LiteralPath $comparePath -Value $compareOutput -Encoding UTF8

try {
    $report = ConvertFrom-Json -InputObject $compareOutput
}
catch {
    throw "The comparison output is not valid JSON. Review $comparePath. $($_.Exception.Message)"
}

if ($report.schema_version -ne 1) {
    throw "Unexpected comparison JSON schema version: $($report.schema_version)"
}

if ($report.left.events -ne $report.right.events -or
    $report.left.event_types -ne $report.right.event_types) {
    throw "The ETL and CTRA event totals do not match."
}

if ($report.event_type_changes.Count -ne 0 -or
    $report.object_changes.Count -ne 0 -or
    $report.summary.event_types_added -ne 0 -or
    $report.summary.event_types_removed -ne 0 -or
    $report.summary.event_types_count_changed -ne 0 -or
    $report.summary.objects_added -ne 0 -or
    $report.summary.objects_removed -ne 0 -or
    $report.summary.objects_changed -ne 0) {
    throw "The ETL and CTRA comparison reported unexpected differences. Review $comparePath"
}

$leftComTimeline = ConvertTo-Json -InputObject $report.timeline_summary.left_com -Compress -Depth 5
$rightComTimeline = ConvertTo-Json -InputObject $report.timeline_summary.right_com -Compress -Depth 5
$leftOleTimeline = ConvertTo-Json -InputObject $report.timeline_summary.left_ole -Compress -Depth 5
$rightOleTimeline = ConvertTo-Json -InputObject $report.timeline_summary.right_ole -Compress -Depth 5
$leftOperationTimings = ConvertTo-Json -InputObject @($report.timeline.left) -Compress -Depth 5
$rightOperationTimings = ConvertTo-Json -InputObject @($report.timeline.right) -Compress -Depth 5
$leftProcessLifetimes = ConvertTo-Json -InputObject @($report.process_lifetimes.left) -Compress -Depth 8
$rightProcessLifetimes = ConvertTo-Json -InputObject @($report.process_lifetimes.right) -Compress -Depth 8
$leftProcessSummary = ConvertTo-Json -InputObject $report.process_lifecycle_summary.left -Compress -Depth 5
$rightProcessSummary = ConvertTo-Json -InputObject $report.process_lifecycle_summary.right -Compress -Depth 5

if ($leftComTimeline -ne $rightComTimeline -or
    $leftOleTimeline -ne $rightOleTimeline -or
    $leftOperationTimings -ne $rightOperationTimings -or
    $leftProcessLifetimes -ne $rightProcessLifetimes -or
    $leftProcessSummary -ne $rightProcessSummary) {
    throw "ETL and CTRA timeline summaries do not match."
}

$oleFixtureReport = $null
if ($captureNewTrace) {
    $oleFixtureReport = $oleFixturePath
}

$summary = [ordered]@{
    result = "PASS"
    etl_path = $EtlPath
    archive_path = $archivePath
    events = $report.left.events
    event_types = $report.left.event_types
    process_start_events = $processStartCount
    process_end_events = $processEndCount
    process_decode_failures = $processDecodeFailures
    process_graph_known_generations = $processGraphKnownGenerations
    process_graph_unknown_generations = $processGraphUnknownGenerations
    com_events = $comEventCount
    com_rundown_validation_status = $rundownValidationStatus
    com_rundown_events = $rundownEventCount
    com_rundown_identity_values_reported = $rundownIdentityValueCount
    com_runtime_events = $comRuntimeEventCount
    com_premature_stub_rundown_events = $comRuntimeStubRundownCount
    rpc_events = $rpcEventCount
    rpc_classified_events = $rpcClassifiedEventCount
    rpc_client_call_start_events = $rpcClientCallStartCount
    rpc_client_call_stop_events = $rpcClientCallStopCount
    rpc_server_call_start_events = $rpcServerCallStartCount
    rpc_server_call_stop_events = $rpcServerCallStopCount
    rpc_function_trace_start_events = $rpcFunctionTraceStartCount
    rpc_function_trace_stop_events = $rpcFunctionTraceStopCount
    rpc_function_trace_endpoint_events = $rpcFunctionTraceEndpointCount
    rpcss_events = $rpcssEventCount
    rpcss_classified_events = $rpcssClassifiedEventCount
    ole_validation_status = $oleValidationStatus
    ole_events = $oleEventCount
    ole_clipboard_operations = $oleClipboardOperations
    ole_drag_drop_operations = $oleDragDropOperations
    ole_timeline_matched = $oleTimelineMatched
    ole_timeline_matched_by_activity_id = $oleTimelineMatchedByActivity
    ole_timeline_matched_by_process_thread = $oleTimelineMatchedByProcessThread
    ole_timeline_missing_activity_id = $oleTimelineMissingActivity
    ole_payload_properties_reported = $olePayloadPropertyCount
    ole_data_object_properties_reported = $oleDataObjectPropertyCount
    identity_values_reported = $identityValueCount
    com_timeline_matched = $comTimelineMatched
    com_timeline_matched_by_call_trace_id = $comTimelineMatchedByCallTrace
    com_timeline_matched_by_activity_id = $comTimelineMatchedByActivity
    com_timeline_matched_by_process_thread = $comTimelineMatchedByProcessThread
    com_timeline_missing_call_trace_id = $comTimelineMissingCallTrace
    com_timeline_missing_activity_id = $comTimelineMissingActivity
    com_activations_succeeded = $activationCount
    com_activation_failures = @($activationFailures.ToArray())
    checksum = "CRC32 valid"
    comparison_schema_version = $report.schema_version
    inspect_report = $inspectPath
    com_search_report = $searchPath
    com_rundown_search_report = $rundownSearchPath
    com_runtime_search_report = $comRuntimeSearchPath
    rpc_search_report = $rpcSearchPath
    rpcss_search_report = $rpcssSearchPath
    ole_search_report = $oleSearchPath
    ole_fixture_report = $oleFixtureReport
    archive_inspect_report = $archiveInspectPath
    comparison_report = $comparePath
}

$summary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $summaryPath -Encoding UTF8

Write-Host ""
Write-Host "Real ETL validation: PASS"
Write-Host "Events: $($report.left.events)"
Write-Host "Distinct event types: $($report.left.event_types)"
Write-Host "Kernel Process Start events: $processStartCount"
Write-Host "Kernel Process End events: $processEndCount"
Write-Host "Kernel Process decode failures: $processDecodeFailures"
Write-Host "Graph Process generations: known=$processGraphKnownGenerations unknown=$processGraphUnknownGenerations"
Write-Host "COM Performance events: $comEventCount"
Write-Host "COM rundown validation status: $rundownValidationStatus"
Write-Host "COM rundown events: $rundownEventCount"
Write-Host "COM rundown identity values reported: $rundownIdentityValueCount"
Write-Host "COMRuntime events: $comRuntimeEventCount"
Write-Host "COM PrematureStubRundown events: $comRuntimeStubRundownCount"
Write-Host "RPC events: $rpcEventCount (classified=$rpcClassifiedEventCount)"
Write-Host "RPC client calls: start=$rpcClientCallStartCount stop=$rpcClientCallStopCount"
Write-Host "RPC server calls: start=$rpcServerCallStartCount stop=$rpcServerCallStopCount"
Write-Host "RPC FunctionTrace: start=$rpcFunctionTraceStartCount stop=$rpcFunctionTraceStopCount endpoint=$rpcFunctionTraceEndpointCount"
Write-Host "RPCSS events: $rpcssEventCount (classified=$rpcssClassifiedEventCount)"
Write-Host "OLE validation status: $oleValidationStatus"
Write-Host "OLE Performance events: $oleEventCount"
Write-Host "OLE clipboard operations: $oleClipboardOperations"
Write-Host "OLE drag and drop operations: $oleDragDropOperations"
Write-Host "OLE start/stop pairs matched: $oleTimelineMatched"
Write-Host "OLE pairs matched by ActivityId: $oleTimelineMatchedByActivity"
Write-Host "OLE pairs matched by process/thread/operation: $oleTimelineMatchedByProcessThread"
Write-Host "OLE events missing ActivityId: $oleTimelineMissingActivity"
Write-Host "OLE payload properties reported by TDH: $olePayloadPropertyCount"
Write-Host "OLE properties named DataObject: $oleDataObjectPropertyCount"
Write-Host "Identity values reported: $identityValueCount"
Write-Host "COM start/stop pairs matched: $comTimelineMatched"
Write-Host "Matched by CallTraceId: $comTimelineMatchedByCallTrace"
Write-Host "Matched by ActivityId: $comTimelineMatchedByActivity"
Write-Host "Matched by process/thread/operation: $comTimelineMatchedByProcessThread"
Write-Host "COM events missing CallTraceId: $comTimelineMissingCallTrace"
Write-Host "COM events missing ActivityId: $comTimelineMissingActivity"
Write-Host "CTRA checksum: CRC32 valid"
Write-Host "ETL and CTRA comparison: identical"
Write-Host "Validation summary: $summaryPath"
