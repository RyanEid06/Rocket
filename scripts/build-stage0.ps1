[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
. (Join-Path $projectRoot 'dependencies\activate.ps1')
$buildDirectory = Join-Path $projectRoot ('out\build\windows-stage0-' + $Configuration.ToLowerInvariant())

cmake -S $projectRoot -B $buildDirectory -G Ninja `
    "-DCMAKE_C_COMPILER=$env:ROCKET_MSVC_COMPILER" `
    "-DCMAKE_CXX_COMPILER=$env:ROCKET_MSVC_COMPILER" `
    "-DCMAKE_AR=$env:ROCKET_MSVC_LIBRARIAN" `
    "-DCMAKE_LINKER=$env:ROCKET_MSVC_LINKER" `
    "-DCMAKE_BUILD_TYPE=$Configuration" -DROCKETC_ENABLE_LLVM=OFF
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build $buildDirectory
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
ctest --test-dir $buildDirectory --output-on-failure
exit $LASTEXITCODE
