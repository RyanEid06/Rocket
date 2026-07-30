[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$version = '1.5.0'
$configurationName = $Configuration.ToLowerInvariant()
$buildDirectory = Join-Path $projectRoot "out\build\windows-$configurationName"
$bootstrapDirectory = Join-Path $projectRoot "out\bootstrap\windows-$configurationName"
$packageParent = Join-Path $projectRoot 'out\package'
$packageRoot = Join-Path $packageParent "rocket-$version-windows-x64"
$archive = "$packageRoot.zip"

# Every replaced path is a fixed descendant of out/package.
$resolvedParent = [System.IO.Path]::GetFullPath($packageParent)
$resolvedPackage = [System.IO.Path]::GetFullPath($packageRoot)
if (-not $resolvedPackage.StartsWith(
        $resolvedParent + [System.IO.Path]::DirectorySeparatorChar,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw 'Refusing to package outside out/package.'
}

& powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
    (Join-Path $projectRoot 'scripts\build.ps1') -Configuration $Configuration
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
    (Join-Path $projectRoot 'scripts\bootstrap.ps1') `
    -Configuration $Configuration -SkipStage0Build
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

. (Join-Path $projectRoot 'dependencies\activate.ps1')

$llvmRoot = Join-Path $projectRoot 'dependencies\installed\llvm-22.1.6'
$msvcLibraryDirectory = Join-Path $env:VCToolsInstallDir 'lib\x64'
$ucrtLibraryDirectory = Join-Path $env:WindowsSdkDir `
    ("Lib\{0}ucrt\x64" -f $env:WindowsSDKVersion)
$umLibraryDirectory = Join-Path $env:WindowsSdkDir `
    ("Lib\{0}um\x64" -f $env:WindowsSDKVersion)

$requiredFiles = @(
    (Join-Path $bootstrapDirectory 'stage3.exe')
    (Join-Path $buildDirectory 'rocketc.exe')
    (Join-Path $buildDirectory 'rocket-lsp.exe')
    (Join-Path $buildDirectory 'rocket_runtime.lib')
    (Join-Path $llvmRoot 'bin\clang.exe')
    (Join-Path $llvmRoot 'bin\llvm-lib.exe')
    (Join-Path $llvmRoot 'bin\lld-link.exe')
)
foreach ($required in $requiredFiles) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Missing release input: $required"
    }
}
foreach ($required in $msvcLibraryDirectory, $ucrtLibraryDirectory, $umLibraryDirectory) {
    if (-not (Test-Path -LiteralPath $required -PathType Container)) {
        throw "Missing native library directory: $required"
    }
}

if (Test-Path -LiteralPath $packageRoot) {
    Remove-Item -LiteralPath $packageRoot -Recurse -Force
}
if (Test-Path -LiteralPath $archive) {
    Remove-Item -LiteralPath $archive -Force
}

$binDirectory = New-Item -ItemType Directory -Path (Join-Path $packageRoot 'bin') -Force
$libDirectory = New-Item -ItemType Directory -Path (Join-Path $packageRoot 'lib') -Force
$stage0Directory = New-Item -ItemType Directory -Path (Join-Path $packageRoot 'stage0') -Force

Copy-Item -LiteralPath (Join-Path $bootstrapDirectory 'stage3.exe') `
    -Destination (Join-Path $binDirectory 'rocketc.exe')
Copy-Item -LiteralPath (Join-Path $buildDirectory 'rocket-lsp.exe') `
    -Destination (Join-Path $binDirectory 'rocket-lsp.exe')
Copy-Item -LiteralPath (Join-Path $buildDirectory 'rocketc.exe') `
    -Destination (Join-Path $stage0Directory 'rocketc-stage0.exe')
Copy-Item -LiteralPath (Join-Path $buildDirectory 'rocket_runtime.lib') `
    -Destination (Join-Path $libDirectory 'rocket_runtime.lib')
Copy-Item -LiteralPath (Join-Path $llvmRoot 'bin\clang.exe') -Destination $binDirectory
Copy-Item -LiteralPath (Join-Path $llvmRoot 'bin\llvm-lib.exe') -Destination $binDirectory
Copy-Item -LiteralPath (Join-Path $llvmRoot 'bin\lld-link.exe') -Destination $binDirectory

# Clang locates its compiler-rt builtins relative to bin/../lib/clang.
Copy-Item -LiteralPath (Join-Path $llvmRoot 'lib\clang') `
    -Destination (Join-Path $libDirectory 'clang') -Recurse

foreach ($entry in @(
        @{ Source = $msvcLibraryDirectory; Name = 'msvc' },
        @{ Source = $ucrtLibraryDirectory; Name = 'ucrt' },
        @{ Source = $umLibraryDirectory; Name = 'um' }
    )) {
    $destination = New-Item -ItemType Directory `
        -Path (Join-Path $libDirectory $entry.Name) -Force
    Get-ChildItem -LiteralPath $entry.Source -Filter '*.lib' -File |
        Copy-Item -Destination $destination
}

Copy-Item -LiteralPath (Join-Path $projectRoot 'README.md') -Destination $packageRoot
Copy-Item -LiteralPath (Join-Path $projectRoot 'docs') -Destination $packageRoot -Recurse
Copy-Item -LiteralPath (Join-Path $projectRoot 'editors') -Destination $packageRoot -Recurse
Copy-Item -LiteralPath (Join-Path $projectRoot 'stdlib') -Destination $packageRoot -Recurse
Copy-Item -LiteralPath (Join-Path $bootstrapDirectory 'SHA256SUMS.txt') `
    -Destination (Join-Path $packageRoot 'BOOTSTRAP_SHA256SUMS.txt')

$packageNote = @"
# Rocket 1.5.0 for Windows x64

``bin\rocketc.exe`` is the production self-hosted Rocket compiler. It discovers
the bundled runtime, Clang/LLD, compiler-rt resources, and native link libraries
relative to its own executable, so no Visual Studio, Windows SDK, LLVM, or
activated developer shell is required to compile Rocket programs.

``bin\rocket-lsp.exe`` is the editor-neutral Phase 17 language-server
foundation. It uses LSP 3.17 over standard input/output and supplies live coded
frontend diagnostics without executing builds or package code.

``stage0\rocketc-stage0.exe`` preserves the C++ bootstrap compiler used to
reproduce stage1. ``BOOTSTRAP_SHA256SUMS.txt`` records the deterministic
stage2/stage3 proof; ``SHA256SUMS.txt`` covers every distributed file.

Supported target: Windows x64. See ``docs\RELEASE_1_5.md`` for the standard
library surface, compatibility policy, limitations, and validation matrix.
"@
Set-Content -LiteralPath (Join-Path $packageRoot 'PACKAGE.md') `
    -Value $packageNote -Encoding utf8

& powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
    (Join-Path $projectRoot 'scripts\verify-distribution.ps1') `
    -PackageRoot $packageRoot
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Get-ChildItem -LiteralPath $packageRoot -Recurse -File |
    Sort-Object FullName |
    Get-FileHash -Algorithm SHA256 |
    ForEach-Object {
        "$($_.Hash.ToLowerInvariant())  $($_.Path.Substring($packageRoot.Length + 1))"
    } |
    Set-Content -LiteralPath (Join-Path $packageRoot 'SHA256SUMS.txt') -Encoding ascii

Compress-Archive -LiteralPath $packageRoot -DestinationPath $archive `
    -CompressionLevel Optimal
Write-Output "Packaged self-contained Rocket $version at $archive"
