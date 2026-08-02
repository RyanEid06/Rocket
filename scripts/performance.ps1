[CmdletBinding()]
param(
    [string]$Compiler = '',
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$configurationName = $Configuration.ToLowerInvariant()
if (-not $Compiler) {
    . (Join-Path $projectRoot 'dependencies\activate.ps1')
    $Compiler = Join-Path $projectRoot "out\bootstrap\windows-$configurationName\stage3.exe"
}
$Compiler = [System.IO.Path]::GetFullPath($Compiler)
if (-not (Test-Path -LiteralPath $Compiler -PathType Leaf)) {
    throw "Performance compiler does not exist: $Compiler"
}
$savedRocketStage0 = $env:ROCKET_STAGE0
$stage0Candidate = Join-Path $projectRoot "out\build\windows-$configurationName\rocketc.exe"
if ([string]::IsNullOrWhiteSpace($env:ROCKET_STAGE0) -and
    (Test-Path -LiteralPath $stage0Candidate -PathType Leaf)) {
    $env:ROCKET_STAGE0 = $stage0Candidate
}

$measurements = [System.Collections.Generic.List[object]]::new()
function Measure-RocketCommand {
    param(
        [string]$Name,
        [string[]]$Arguments,
        [double]$MaximumSeconds
    )
    $watch = [System.Diagnostics.Stopwatch]::StartNew()
    & $Compiler @Arguments *> $null
    $status = $LASTEXITCODE
    $watch.Stop()
    if ($status -ne 0) {
        throw "Performance case '$Name' failed with status $status."
    }
    $seconds = [math]::Round($watch.Elapsed.TotalSeconds, 3)
    $measurements.Add([pscustomobject]@{
        name = $Name
        seconds = $seconds
        maximum_seconds = $MaximumSeconds
        passed = ($seconds -le $MaximumSeconds)
    })
    if ($seconds -gt $MaximumSeconds) {
        throw "Performance case '$Name' took ${seconds}s; limit is ${MaximumSeconds}s."
    }
}

try {
    & $Compiler --version *> $null
    if ($LASTEXITCODE -ne 0) { throw 'Compiler warmup failed.' }

    $compilerSource = Join-Path $projectRoot 'compiler\src\main.rocket'
    # The Debug runtime deliberately keeps checks and symbols enabled. Keep its
    # self-analysis ceilings explicit instead of silently applying Release
    # numbers to a different binary configuration.
    # The 1.8 ownership/concurrency closure increased the self-hosted HIR
    # surface and exposed ordinary 117-126 second host variance. Keep a strict
    # versioned ceiling with useful regression margin instead of a flaky limit.
    $hirMaximum = if ($Configuration -eq 'Debug') { 180 } else { 135 }
    $mirMaximum = if ($Configuration -eq 'Debug') { 240 } else { 180 }
    Measure-RocketCommand 'hello-check' @('check', (Join-Path $projectRoot 'examples\hello.rocket')) 5
    Measure-RocketCommand 'hello-build' @('build', (Join-Path $projectRoot 'examples\hello.rocket')) 15
    Measure-RocketCommand 'compiler-hir-self-check' @('--check-hir', $compilerSource) $hirMaximum
    Measure-RocketCommand 'compiler-mir-self-check' @('--check-mir', $compilerSource) $mirMaximum
    Measure-RocketCommand 'native-interop-check' @('check', (Join-Path $projectRoot 'tests\fixtures\phase13_native_package')) 5
    Measure-RocketCommand 'native-library-build' @('build', (Join-Path $projectRoot 'tests\fixtures\phase13_static_library')) 15
    Measure-RocketCommand 'raylib-reference-check' @('check', (Join-Path $projectRoot 'examples\raylib_showcase')) 10
    Measure-RocketCommand 'raylib-reference-build' @('build', (Join-Path $projectRoot 'examples\raylib_showcase')) 30
    Measure-RocketCommand 'phase18-concurrency-check' @('check', (Join-Path $projectRoot 'tests\fixtures\phase18_concurrency.rocket')) 5
    Measure-RocketCommand 'phase18-async-build' @('build', (Join-Path $projectRoot 'tests\fixtures\phase18_nested_await.rocket')) 15
    Measure-RocketCommand 'phase18-task-group-build' @('build', (Join-Path $projectRoot 'tests\fixtures\phase18_task_group.rocket')) 15

    $reportDirectory = Join-Path $projectRoot 'out\performance'
    New-Item -ItemType Directory -Path $reportDirectory -Force | Out-Null
    $reportPath = Join-Path $reportDirectory "rocket-2.0-$configurationName.json"
    $report = [pscustomobject]@{
        version = '2.0.0'
        configuration = $Configuration
        compiler = $Compiler
        sha256 = (Get-FileHash -LiteralPath $Compiler -Algorithm SHA256).Hash.ToLowerInvariant()
        measured_at_utc = [DateTime]::UtcNow.ToString('o')
        measurements = $measurements
    }
    $report | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $reportPath -Encoding utf8
    Write-Output "Rocket 2.0 performance gates passed: $reportPath"
    foreach ($measurement in $measurements) {
        Write-Output ("  {0}: {1}s <= {2}s" -f $measurement.name, $measurement.seconds,
            $measurement.maximum_seconds)
    }
} finally {
    $env:ROCKET_STAGE0 = $savedRocketStage0
}
