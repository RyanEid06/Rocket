$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'activate.ps1')

$checks = @(
    @{ Name = 'Git'; Command = 'git'; Args = @('--version') },
    @{ Name = 'CMake'; Command = 'cmake'; Args = @('--version') },
    @{ Name = 'Ninja'; Command = 'ninja'; Args = @('--version') },
    @{ Name = 'Clang'; Command = 'clang'; Args = @('--version') },
    @{ Name = 'LLVM config'; Command = 'llvm-config'; Args = @('--version') },
    @{ Name = 'MSVC'; Command = 'cl'; Args = @() }
)

foreach ($check in $checks) {
    $command = Get-Command $check.Command -ErrorAction SilentlyContinue
    if (-not $command) { throw "$($check.Name) was not found after activation." }
    $firstLine = & $check.Command @($check.Args) 2>&1 | Select-Object -First 1
    Write-Host ('{0,-12} {1}' -f ($check.Name + ':'), $firstLine)
}

if (-not (Test-Path -LiteralPath $env:LLVM_DIR)) { throw "LLVM CMake package not found at $env:LLVM_DIR" }
Write-Host 'Toolchain verification passed.'
