[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
. (Join-Path $projectRoot 'dependencies\activate.ps1')
$preset = 'windows-' + $Configuration.ToLowerInvariant()

cmake --preset $preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build --preset $preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
ctest --preset $preset -L phase14 --output-on-failure
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Output "Rocket raylib $Configuration validation passed."
