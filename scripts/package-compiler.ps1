[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [switch]$SkipToolchain
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$version = '0.8.0'
$configurationName = $Configuration.ToLowerInvariant()
$buildDirectory = Join-Path $projectRoot "out\build\windows-$configurationName"
$packageParent = Join-Path $projectRoot 'out\package'
$packageRoot = Join-Path $packageParent "rocket-$version-windows-x64"
$archive = "$packageRoot.zip"

# Both destructive targets are fixed descendants of out/package.
$resolvedParent = [System.IO.Path]::GetFullPath($packageParent)
$resolvedPackage = [System.IO.Path]::GetFullPath($packageRoot)
if (-not $resolvedPackage.StartsWith($resolvedParent + [System.IO.Path]::DirectorySeparatorChar,
                                     [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Refusing to package outside out/package.'
}

& powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
    (Join-Path $projectRoot 'scripts\build.ps1') -Configuration $Configuration
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if (Test-Path -LiteralPath $packageRoot) {
    Remove-Item -LiteralPath $packageRoot -Recurse -Force
}
if (Test-Path -LiteralPath $archive) {
    Remove-Item -LiteralPath $archive -Force
}
New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null

Copy-Item -LiteralPath (Join-Path $buildDirectory 'rocketc.exe') -Destination $packageRoot
Copy-Item -LiteralPath (Join-Path $buildDirectory 'rocket_runtime.lib') -Destination $packageRoot
Copy-Item -LiteralPath (Join-Path $projectRoot 'README.md') -Destination $packageRoot
Copy-Item -LiteralPath (Join-Path $projectRoot 'docs') -Destination $packageRoot -Recurse
Copy-Item -LiteralPath (Join-Path $projectRoot 'editors') -Destination $packageRoot -Recurse

if (-not $SkipToolchain) {
    $llvmBin = Join-Path $projectRoot 'dependencies\installed\llvm-22.1.6\bin'
    $toolchain = Join-Path $packageRoot 'toolchain'
    New-Item -ItemType Directory -Path $toolchain -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $llvmBin 'clang.exe') -Destination $toolchain
    Copy-Item -LiteralPath (Join-Path $llvmBin 'lld-link.exe') -Destination $toolchain
}

$packageNote = @"
# Rocket compiler developer package $version

Run ``rocketc.exe --version`` to verify the compiler. The bundled runtime library
is discovered beside the compiler and the optional Clang driver under
``toolchain`` is preferred over the development-tree fallback.

This pre-1.0 Windows x64 package still requires the MSVC Build Tools and Windows
SDK environment for native linking. A fully self-contained compiler is the
Rocket 1.0 distribution milestone, not a Phase 8 claim.
"@
Set-Content -LiteralPath (Join-Path $packageRoot 'PACKAGE.md') -Value $packageNote -Encoding utf8

Get-ChildItem -LiteralPath $packageRoot -Recurse -File |
    Sort-Object FullName |
    Get-FileHash -Algorithm SHA256 |
    ForEach-Object { "$($_.Hash.ToLowerInvariant())  $($_.Path.Substring($packageRoot.Length + 1))" } |
    Set-Content -LiteralPath (Join-Path $packageRoot 'SHA256SUMS.txt') -Encoding ascii

Compress-Archive -LiteralPath $packageRoot -DestinationPath $archive -CompressionLevel Optimal
Write-Output "Packaged Rocket $version at $archive"
