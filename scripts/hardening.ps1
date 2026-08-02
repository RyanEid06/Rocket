[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [string]$Compiler = '',
    [switch]$Sanitizers
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$configurationName = $Configuration.ToLowerInvariant()
$preset = "windows-$configurationName"
if ($Sanitizers) { $preset = 'windows-asan' }

. (Join-Path $projectRoot 'dependencies\activate.ps1')
if ($Sanitizers) {
    cmake --preset $preset
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    cmake --build --preset $preset
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$buildDirectory = Join-Path $projectRoot "out\build\$preset"
if (-not $Compiler) { $Compiler = Join-Path $buildDirectory 'rocketc.exe' }
$Compiler = [System.IO.Path]::GetFullPath($Compiler)
if (-not (Test-Path -LiteralPath $Compiler -PathType Leaf)) {
    throw "Hardening compiler does not exist: $Compiler"
}

$savedAsanOptions = $env:ASAN_OPTIONS
$savedStage0 = $env:ROCKET_STAGE0
$savedFrontendCases = $env:ROCKET_HARDENING_FRONTEND_CASES
$savedManifestCases = $env:ROCKET_HARDENING_MANIFEST_CASES
$reportConfiguration = if ($Sanitizers) { 'AddressSanitizer' } else { $Configuration }
try {
    if ($Sanitizers) {
        # MSVC AddressSanitizer does not implement LeakSanitizer on Windows.
        $env:ASAN_OPTIONS = 'abort_on_error=1:strict_string_checks=1'
    }
    $env:ROCKET_HARDENING_FRONTEND_CASES = '2000'
    $env:ROCKET_HARDENING_MANIFEST_CASES = '256'
    $env:ROCKET_STAGE0 = $Compiler
    ctest --test-dir $buildDirectory -L phase20 --output-on-failure
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    $reportDirectory = Join-Path $projectRoot 'out\hardening'
    New-Item -ItemType Directory -Path $reportDirectory -Force | Out-Null
    $reportPath = Join-Path $reportDirectory "$preset.json"
    [pscustomobject]@{
        schema = 'rocket-hardening-1'
        version = '2.0.0'
        target = 'windows-x64'
        configuration = $reportConfiguration
        deterministic_fuzz_seed = '0x524f434b45543230'
        frontend_cases = 2000
        manifest_cases = 256
        sanitizers = [bool]$Sanitizers
        compiler_sha256 = (Get-FileHash -LiteralPath $Compiler -Algorithm SHA256).Hash.ToLowerInvariant()
    } | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath $reportPath -Encoding utf8
    Write-Output "Rocket 2.0 hardening gate passed: $reportPath"
} finally {
    $env:ASAN_OPTIONS = $savedAsanOptions
    $env:ROCKET_STAGE0 = $savedStage0
    $env:ROCKET_HARDENING_FRONTEND_CASES = $savedFrontendCases
    $env:ROCKET_HARDENING_MANIFEST_CASES = $savedManifestCases
}
