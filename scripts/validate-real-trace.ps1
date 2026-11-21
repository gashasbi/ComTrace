param(
    [Parameter(Mandatory = $true)]
    [string]$ComTraceExe,

    [Parameter(Mandatory = $true)]
    [string]$ComTraceArchiveExe,

    [string]$EtlPath,

    [string]$OutputDirectory = (Join-Path (Get-Location).Path "comtrace-validation"),

    [ValidateRange(10, 3600)]
    [int]$CaptureSeconds = 30
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

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$OutputDirectory = (Resolve-Path -LiteralPath $OutputDirectory).Path

$runId = Get-Date -Format "yyyyMMdd-HHmmss"
$archivePath = Join-Path $OutputDirectory "comtrace-$runId.ctra"
$inspectPath = Join-Path $OutputDirectory "comtrace-$runId-inspect.txt"
$searchPath = Join-Path $OutputDirectory "comtrace-$runId-com-search.txt"
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
$missingCallTrace = [regex]::Match($searchOutput, "Missing CallTraceId:\s*(\d+)")
if (-not $matchedTimeline.Success -or -not $missingCallTrace.Success) {
    throw "The COM timeline summary did not contain its pairing counters. Review $searchPath"
}

$comTimelineMatched = [UInt64]::Parse($matchedTimeline.Groups[1].Value)
$comTimelineMissingCallTrace = [UInt64]::Parse($missingCallTrace.Groups[1].Value)
if ($comTimelineMatched -eq 0) {
    throw "No COM start/stop pairs were matched. Review CallTraceId property schemas in $searchPath"
}

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

if ($leftComTimeline -ne $rightComTimeline -or
    $leftOleTimeline -ne $rightOleTimeline -or
    $leftOperationTimings -ne $rightOperationTimings) {
    throw "ETL and CTRA timeline summaries do not match."
}

$summary = [ordered]@{
    result = "PASS"
    etl_path = $EtlPath
    archive_path = $archivePath
    events = $report.left.events
    event_types = $report.left.event_types
    com_events = $comEventCount
    identity_values_reported = $identityValueCount
    com_timeline_matched = $comTimelineMatched
    com_timeline_missing_call_trace_id = $comTimelineMissingCallTrace
    com_activations_succeeded = $activationCount
    com_activation_failures = @($activationFailures.ToArray())
    checksum = "CRC32 valid"
    comparison_schema_version = $report.schema_version
    inspect_report = $inspectPath
    com_search_report = $searchPath
    archive_inspect_report = $archiveInspectPath
    comparison_report = $comparePath
}

$summary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $summaryPath -Encoding UTF8

Write-Host ""
Write-Host "Real ETL validation: PASS"
Write-Host "Events: $($report.left.events)"
Write-Host "Distinct event types: $($report.left.event_types)"
Write-Host "COM Performance events: $comEventCount"
Write-Host "Identity values reported: $identityValueCount"
Write-Host "COM start/stop pairs matched: $comTimelineMatched"
Write-Host "COM events missing CallTraceId: $comTimelineMissingCallTrace"
Write-Host "CTRA checksum: CRC32 valid"
Write-Host "ETL and CTRA comparison: identical"
Write-Host "Validation summary: $summaryPath"
