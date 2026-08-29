[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [ValidateSet('local', 'nightly', 'preview', 'stable')]
    [string]$ReleaseChannel = 'local',
    [string]$SigningCertificateThumbprint = '',
    [switch]$Official,
    [switch]$SkipArchiveReproducibility
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$version = '2.0.0'
$configurationName = $Configuration.ToLowerInvariant()
$buildDirectory = Join-Path $projectRoot "out\build\windows-$configurationName"
$bootstrapDirectory = Join-Path $projectRoot "out\bootstrap\windows-$configurationName"
$packageParent = Join-Path $projectRoot 'out\package'
$packageRoot = Join-Path $packageParent "rocket-$version-windows-x64"
$archive = "$packageRoot.zip"
$repeatArchive = "$packageRoot.repeat.zip"

if ($Official -and ($ReleaseChannel -ne 'stable' -or
        [string]::IsNullOrWhiteSpace($SigningCertificateThumbprint))) {
    throw 'Official releases require -ReleaseChannel stable and a signing certificate thumbprint.'
}

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
if (Test-Path -LiteralPath $repeatArchive) {
    Remove-Item -LiteralPath $repeatArchive -Force
}

$binDirectory = New-Item -ItemType Directory -Path (Join-Path $packageRoot 'bin') -Force
$libDirectory = New-Item -ItemType Directory -Path (Join-Path $packageRoot 'lib') -Force
$stage0Directory = New-Item -ItemType Directory -Path (Join-Path $packageRoot 'stage0') -Force
$toolsDirectory = New-Item -ItemType Directory -Path (Join-Path $packageRoot 'tools') -Force

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
Copy-Item -LiteralPath (Join-Path $projectRoot 'SECURITY.md') -Destination $packageRoot
Copy-Item -LiteralPath (Join-Path $projectRoot 'CONTRIBUTING.md') -Destination $packageRoot
Copy-Item -LiteralPath (Join-Path $projectRoot 'docs') -Destination $packageRoot -Recurse
Copy-Item -LiteralPath (Join-Path $projectRoot 'editors') -Destination $packageRoot -Recurse
Copy-Item -LiteralPath (Join-Path $projectRoot 'stdlib') -Destination $packageRoot -Recurse
Copy-Item -LiteralPath (Join-Path $bootstrapDirectory 'SHA256SUMS.txt') `
    -Destination (Join-Path $packageRoot 'BOOTSTRAP_SHA256SUMS.txt')
foreach ($tool in 'debugging.ps1', 'tooling.ps1', 'repl-prototype.ps1',
        'compatibility.ps1', 'hardening.ps1', 'minimize-crash.ps1',
        'application-validation.ps1') {
    Copy-Item -LiteralPath (Join-Path $projectRoot "scripts\$tool") -Destination $toolsDirectory
}

$packageNote = @"
# Rocket 2.0.0 for Windows x64

``bin\rocketc.exe`` is the production self-hosted Rocket compiler. It discovers
the bundled runtime, Clang/LLD, compiler-rt resources, and native link libraries
relative to its own executable, so no Visual Studio, Windows SDK, LLVM, or
activated developer shell is required to compile Rocket programs.

``bin\rocket-lsp.exe`` is the editor-neutral Phase 17 semantic language server.
It uses LSP 3.17 over standard input/output and never executes builds or package
code merely because source is opened. ``tools`` contains the debug, coverage,
profile, benchmark, compatibility, hardening, crash-minimization, application,
and incremental-AOT validation workflows.

``stage0\rocketc-stage0.exe`` preserves the C++ bootstrap compiler used to
reproduce stage1. ``BOOTSTRAP_SHA256SUMS.txt`` records the deterministic
stage2/stage3 proof; ``RELEASE-PROVENANCE.json`` records the source/tool
identity; and ``SHA256SUMS.txt`` covers every distributed file except itself
and its optional detached signature.

This Rocket 2.0 archive supports Windows x64. Rocket 2.1 portable packages are
documented separately in ``docs\RELEASE_2_1.md``.
See ``docs\RELEASE_2_0.md``, ``SECURITY.md``, and
``docs\PROJECT_CONTEXT.md`` for the frozen contracts, support policy,
limitations, and validation matrix. Release channel: $ReleaseChannel.
"@
$utf8NoBom = [System.Text.UTF8Encoding]::new($false)
[System.IO.File]::WriteAllText(
    (Join-Path $packageRoot 'PACKAGE.md'), $packageNote + "`n", $utf8NoBom)

$gitCommit = (& git -C $projectRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or -not $gitCommit) {
    throw 'Could not determine the release source commit.'
}
$gitCommitEpoch = [long]((& git -C $projectRoot show -s --format=%ct HEAD).Trim())
if ($LASTEXITCODE -ne 0 -or $gitCommitEpoch -le 0) {
    throw 'Could not determine the release source timestamp.'
}
$workingTreeState = if ((& git -C $projectRoot status --porcelain).Count -eq 0) {
    'clean'
} else {
    'dirty'
}
if ($Official -and $workingTreeState -ne 'clean') {
    throw 'Official releases require a clean Git working tree.'
}

$certificate = $null
if (-not [string]::IsNullOrWhiteSpace($SigningCertificateThumbprint)) {
    $normalizedThumbprint = $SigningCertificateThumbprint.Replace(' ', '').ToUpperInvariant()
    $certificatePath = "Cert:\CurrentUser\My\$normalizedThumbprint"
    if (-not (Test-Path -LiteralPath $certificatePath)) {
        throw "Signing certificate is unavailable: $normalizedThumbprint"
    }
    $certificate = Get-Item -LiteralPath $certificatePath
    if (-not $certificate.HasPrivateKey) {
        throw 'Release signing certificate has no private key.'
    }
    $signTool = Join-Path $env:WindowsSdkDir `
        ("bin\{0}x64\signtool.exe" -f $env:WindowsSDKVersion)
    if (-not (Test-Path -LiteralPath $signTool -PathType Leaf)) {
        throw "Windows SignTool is unavailable: $signTool"
    }
    foreach ($signedFile in @(
            (Join-Path $binDirectory 'rocketc.exe'),
            (Join-Path $binDirectory 'rocket-lsp.exe'),
            (Join-Path $stage0Directory 'rocketc-stage0.exe'))) {
        & $signTool sign /sha1 $normalizedThumbprint /fd SHA256 $signedFile
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
        if ($Official -and
            (Get-AuthenticodeSignature -LiteralPath $signedFile).Status -ne 'Valid') {
            throw "Official Authenticode validation failed: $signedFile"
        }
    }
}

$provenance = [ordered]@{
    schema = 'rocket-release-provenance-1'
    version = $version
    target = 'windows-x64'
    channel = $ReleaseChannel
    official = [bool]$Official
    source_commit = $gitCommit
    source_commit_epoch = $gitCommitEpoch
    working_tree = $workingTreeState
    archive_format = 'deterministic-zip-1'
    signed = ($null -ne $certificate)
    signing_certificate_thumbprint = if ($certificate) { $certificate.Thumbprint } else { '' }
    compiler_sha256 = (Get-FileHash -LiteralPath (Join-Path $binDirectory 'rocketc.exe') -Algorithm SHA256).Hash.ToLowerInvariant()
    stage0_sha256 = (Get-FileHash -LiteralPath (Join-Path $stage0Directory 'rocketc-stage0.exe') -Algorithm SHA256).Hash.ToLowerInvariant()
    runtime_sha256 = (Get-FileHash -LiteralPath (Join-Path $libDirectory 'rocket_runtime.lib') -Algorithm SHA256).Hash.ToLowerInvariant()
    bootstrap_proof_sha256 = (Get-FileHash -LiteralPath (Join-Path $packageRoot 'BOOTSTRAP_SHA256SUMS.txt') -Algorithm SHA256).Hash.ToLowerInvariant()
    llvm_version = '22.1.6'
    runtime_abi = 1
}
[System.IO.File]::WriteAllText(
    (Join-Path $packageRoot 'RELEASE-PROVENANCE.json'),
    (($provenance | ConvertTo-Json -Depth 4) + "`n"), $utf8NoBom)

$checksumPath = Join-Path $packageRoot 'SHA256SUMS.txt'
$checksumLines = Get-ChildItem -LiteralPath $packageRoot -Recurse -File |
    Where-Object { $_.Name -ne 'SHA256SUMS.txt' -and
        $_.Name -ne 'SHA256SUMS.txt.p7s' } |
    Sort-Object FullName |
    Get-FileHash -Algorithm SHA256 |
    ForEach-Object {
        $relative = $_.Path.Substring($packageRoot.Length + 1).Replace('\', '/')
        "$($_.Hash.ToLowerInvariant())  $relative"
    }
[System.IO.File]::WriteAllLines($checksumPath, $checksumLines,
    [System.Text.Encoding]::ASCII)

if ($certificate) {
    Add-Type -AssemblyName System.Security
    $checksumBytes = [System.IO.File]::ReadAllBytes($checksumPath)
    $content = [System.Security.Cryptography.Pkcs.ContentInfo]::new($checksumBytes)
    $signed = [System.Security.Cryptography.Pkcs.SignedCms]::new($content, $true)
    $signer = [System.Security.Cryptography.Pkcs.CmsSigner]::new($certificate)
    $signer.IncludeOption = [System.Security.Cryptography.X509Certificates.X509IncludeOption]::EndCertOnly
    $signed.ComputeSignature($signer)
    [System.IO.File]::WriteAllBytes(
        (Join-Path $packageRoot 'SHA256SUMS.txt.p7s'), $signed.Encode())
}

& powershell.exe -NoProfile -ExecutionPolicy Bypass -File `
    (Join-Path $projectRoot 'scripts\verify-distribution.ps1') `
    -PackageRoot $packageRoot
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
function New-DeterministicRocketArchive {
    param([string]$Source, [string]$Destination, [DateTimeOffset]$Timestamp)
    if (Test-Path -LiteralPath $Destination) {
        Remove-Item -LiteralPath $Destination -Force
    }
    $stream = [System.IO.File]::Open(
        $Destination, [System.IO.FileMode]::CreateNew,
        [System.IO.FileAccess]::ReadWrite, [System.IO.FileShare]::None)
    try {
        $zip = [System.IO.Compression.ZipArchive]::new(
            $stream, [System.IO.Compression.ZipArchiveMode]::Create, $true)
        try {
            $prefix = (Split-Path $Source -Leaf) + '/'
            foreach ($file in (Get-ChildItem -LiteralPath $Source -Recurse -File |
                    Sort-Object FullName)) {
                $relative = $file.FullName.Substring($Source.Length + 1).Replace('\', '/')
                $entry = $zip.CreateEntry(
                    $prefix + $relative,
                    [System.IO.Compression.CompressionLevel]::Optimal)
                $entry.LastWriteTime = $Timestamp
                $input = [System.IO.File]::OpenRead($file.FullName)
                $output = $entry.Open()
                try { $input.CopyTo($output) }
                finally { $output.Dispose(); $input.Dispose() }
            }
        } finally {
            $zip.Dispose()
        }
    } finally {
        $stream.Dispose()
    }
}

$archiveTimestamp = [DateTimeOffset]::FromUnixTimeSeconds($gitCommitEpoch)
New-DeterministicRocketArchive -Source $packageRoot -Destination $archive `
    -Timestamp $archiveTimestamp
if (-not $SkipArchiveReproducibility) {
    New-DeterministicRocketArchive -Source $packageRoot -Destination $repeatArchive `
        -Timestamp $archiveTimestamp
    $firstHash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash
    $secondHash = (Get-FileHash -LiteralPath $repeatArchive -Algorithm SHA256).Hash
    if ($firstHash -ne $secondHash) {
        throw 'Deterministic archive reproduction produced different SHA-256 values.'
    }
    Remove-Item -LiteralPath $repeatArchive -Force
}
$archiveHash = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant()
Write-Output "Packaged self-contained Rocket $version at $archive"
Write-Output "Archive SHA-256: $archiveHash"
