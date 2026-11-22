param(
    [Parameter(Mandatory = $true)]
    [string]$ConfigurationDirectory,

    [Parameter(Mandatory = $true)]
    [string]$ReleaseTag,

    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory
)

$ErrorActionPreference = "Stop"

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$configurationDirectory = (Resolve-Path -LiteralPath $ConfigurationDirectory).Path

New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$OutputDirectory = (Resolve-Path -LiteralPath $OutputDirectory).Path

$exe = Join-Path $configurationDirectory "ComTrace.exe"
$archiveExe = Join-Path $configurationDirectory "ComTraceArchive.exe"
$oleFixtureExe = Join-Path $configurationDirectory "ComTraceOleActivityFixture.exe"
$lifetimeFixtureExe = Join-Path $configurationDirectory "ComTraceComLifetimeFixture.exe"
$lifetimeLibrary = Join-Path $configurationDirectory "ComTraceLifetimeInstrumentation.lib"
$lifetimeHeader = Join-Path $repositoryRoot "include/comtrace_lifetime.h"

foreach ($file in @($exe, $archiveExe, $oleFixtureExe, $lifetimeFixtureExe, $lifetimeLibrary, $lifetimeHeader, (Join-Path $repositoryRoot "README.md"), (Join-Path $repositoryRoot "LICENSE"))) {
    if (-not (Test-Path -LiteralPath $file -PathType Leaf)) {
        throw "Required release file not found: $file"
    }
}

foreach ($directory in @("docs", "scripts")) {
    if (-not (Test-Path -LiteralPath (Join-Path $repositoryRoot $directory) -PathType Container)) {
        throw "Required release directory not found: $directory"
    }
}

$stage = Join-Path ([IO.Path]::GetTempPath()) ("ComTrace-release-" + [Guid]::NewGuid().ToString("N"))
$zipName = "ComTrace-$ReleaseTag-win-x64.zip"
$zipPath = Join-Path $OutputDirectory $zipName
$exeAsset = Join-Path $OutputDirectory "ComTrace.exe"
$archiveAsset = Join-Path $OutputDirectory "ComTraceArchive.exe"
$oleFixtureAsset = Join-Path $OutputDirectory "ComTraceOleActivityFixture.exe"
$checksumPath = Join-Path $OutputDirectory "SHA256SUMS.txt"

try {
    New-Item -ItemType Directory -Path $stage -Force | Out-Null
    Copy-Item -Path @($exe, $archiveExe, $oleFixtureExe, $lifetimeFixtureExe, (Join-Path $repositoryRoot "README.md"), (Join-Path $repositoryRoot "LICENSE")) -Destination $stage
    New-Item -ItemType Directory -Path (Join-Path $stage "include") -Force | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $stage "lib") -Force | Out-Null
    Copy-Item -LiteralPath $lifetimeHeader -Destination (Join-Path $stage "include")
    Copy-Item -LiteralPath $lifetimeLibrary -Destination (Join-Path $stage "lib")
    Copy-Item -Path @((Join-Path $repositoryRoot "docs"), (Join-Path $repositoryRoot "scripts")) -Destination $stage -Recurse

    if (Test-Path -LiteralPath $zipPath) {
        Remove-Item -LiteralPath $zipPath -Force
    }

    Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zipPath -CompressionLevel Optimal

    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $package = [System.IO.Compression.ZipFile]::OpenRead($zipPath)

    try {
        $entries = @($package.Entries | ForEach-Object { $_.FullName })
        foreach ($requiredEntry in @("ComTraceOleActivityFixture.exe", "ComTraceComLifetimeFixture.exe", "include/comtrace_lifetime.h", "lib/ComTraceLifetimeInstrumentation.lib", "docs/COM_LIFETIME_INSTRUMENTATION.md", "docs/REAL_TRACE_VALIDATION.md", "docs/CTRA_FORMAT.md", "docs/COMPARISON_JSON.md", "scripts/validate-real-trace.ps1")) {
            if ($entries -notcontains $requiredEntry) {
                throw "Release package is missing $requiredEntry"
            }
        }
    }
    finally {
        $package.Dispose()
    }

    Copy-Item -LiteralPath $exe -Destination $exeAsset -Force
    Copy-Item -LiteralPath $archiveExe -Destination $archiveAsset -Force
    Copy-Item -LiteralPath $oleFixtureExe -Destination $oleFixtureAsset -Force

    $checksumLines = @()
    foreach ($file in @($zipPath, $exeAsset, $archiveAsset, $oleFixtureAsset)) {
        $hash = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant()
        $checksumLines += "$hash  $(Split-Path -Path $file -Leaf)"
    }

    $checksumLines | Set-Content -LiteralPath $checksumPath -Encoding ascii
}
finally {
    if (Test-Path -LiteralPath $stage) {
        Remove-Item -LiteralPath $stage -Recurse -Force
    }
}

Write-Host "Release package: $zipPath"
Write-Host "SHA-256 checksums: $checksumPath"
