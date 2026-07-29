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

& $Compiler --version *> $null
if ($LASTEXITCODE -ne 0) { throw 'Compiler warmup failed.' }

$compilerSource = Join-Path $projectRoot 'compiler\src\main.rocket'
Measure-RocketCommand 'hello-check' @('check', (Join-Path $projectRoot 'examples\hello.rocket')) 5
Measure-RocketCommand 'hello-build' @('build', (Join-Path $projectRoot 'examples\hello.rocket')) 15
Measure-RocketCommand 'compiler-hir-self-check' @('--check-hir', $compilerSource) 120
Measure-RocketCommand 'compiler-mir-self-check' @('--check-mir', $compilerSource) 180

$reportDirectory = Join-Path $projectRoot 'out\performance'
New-Item -ItemType Directory -Path $reportDirectory -Force | Out-Null
$reportPath = Join-Path $reportDirectory "rocket-1.1-$configurationName.json"
$report = [pscustomobject]@{
    version = '1.1.0'
    configuration = $Configuration
    compiler = $Compiler
    sha256 = (Get-FileHash -LiteralPath $Compiler -Algorithm SHA256).Hash.ToLowerInvariant()
    measured_at_utc = [DateTime]::UtcNow.ToString('o')
    measurements = $measurements
}
$report | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $reportPath -Encoding utf8
Write-Output "Rocket 1.1 performance gates passed: $reportPath"
foreach ($measurement in $measurements) {
    Write-Output ("  {0}: {1}s <= {2}s" -f $measurement.name, $measurement.seconds,
        $measurement.maximum_seconds)
}
