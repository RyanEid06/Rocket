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
    $savedErrorActionPreference = $ErrorActionPreference
    try {
        # Some native tools, notably cl.exe, print their version banner to stderr.
        # Capture it without allowing Windows PowerShell to promote it to a terminating error.
        $ErrorActionPreference = 'Continue'
        $output = & $check.Command @($check.Args) 2>&1
        $status = $LASTEXITCODE
        $firstLine = $output |
            ForEach-Object { $_.ToString() } |
            Select-Object -First 1
    }
    finally {
        $ErrorActionPreference = $savedErrorActionPreference
    }
    if ($status -ne 0) {
        throw "$($check.Name) version check failed with status $status`: $firstLine"
    }
    Write-Host ('{0,-12} {1}' -f ($check.Name + ':'), $firstLine)
}

if (-not (Test-Path -LiteralPath $env:LLVM_DIR)) { throw "LLVM CMake package not found at $env:LLVM_DIR" }
$manifest = Get-Content -LiteralPath (Join-Path $PSScriptRoot 'manifest.json') -Raw |
    ConvertFrom-Json
$raylibHeader = Join-Path -Path $PSScriptRoot -ChildPath `
    ("installed\{0}\src\raylib.h" -f $manifest.portable.raylib.installDirectory)
if (-not (Test-Path -LiteralPath $raylibHeader -PathType Leaf)) {
    throw "Pinned raylib source was not found at $raylibHeader"
}
$raylibVersionParts = $manifest.portable.raylib.version.Split('.')
$raylibMajorMatch = Select-String -LiteralPath $raylibHeader -Pattern `
    ("^#define RAYLIB_VERSION_MAJOR\s+{0}$" -f $raylibVersionParts[0])
$raylibMinorMatch = Select-String -LiteralPath $raylibHeader -Pattern `
    ("^#define RAYLIB_VERSION_MINOR\s+{0}$" -f $raylibVersionParts[1])
if (-not $raylibMajorMatch -or -not $raylibMinorMatch) {
    throw "Installed raylib header does not report version $($manifest.portable.raylib.version)."
}
Write-Host ('{0,-12} {1}' -f 'raylib:', $manifest.portable.raylib.version)
Write-Host 'Toolchain verification passed.'
