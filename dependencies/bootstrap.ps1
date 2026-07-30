[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$dependencyRoot = $PSScriptRoot
$manifest = Get-Content -LiteralPath (Join-Path $dependencyRoot 'manifest.json') -Raw | ConvertFrom-Json
$cacheRoot = Join-Path $dependencyRoot 'cache'
$installedRoot = Join-Path $dependencyRoot 'installed'
New-Item -ItemType Directory -Force -Path $cacheRoot, $installedRoot | Out-Null

function Get-VerifiedArchive {
    param([Parameter(Mandatory)]$Package)

    $archivePath = Join-Path $cacheRoot $Package.archive
    $valid = $false
    if (Test-Path -LiteralPath $archivePath) {
        $length = (Get-Item -LiteralPath $archivePath).Length
        if ($length -eq $Package.size) {
            $hash = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
            $valid = $hash -eq $Package.sha256
            if (-not $valid) { Remove-Item -LiteralPath $archivePath -Force }
        } elseif ($length -gt $Package.size) {
            Remove-Item -LiteralPath $archivePath -Force
        }
    }

    if (-not $valid) {
        & curl.exe -L --fail --retry 20 --retry-delay 10 --continue-at - --output $archivePath $Package.url
        if ($LASTEXITCODE -ne 0) { throw "Download failed: $($Package.url)" }
    }

    $actual = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
    if ($actual -ne $Package.sha256) {
        throw "Checksum mismatch for $($Package.archive). Expected $($Package.sha256), received $actual"
    }
    return $archivePath
}

$ninja = $manifest.portable.ninja
$ninjaTarget = Join-Path $installedRoot $ninja.installDirectory
if (-not (Test-Path -LiteralPath (Join-Path $ninjaTarget 'ninja.exe'))) {
    $archive = Get-VerifiedArchive $ninja
    New-Item -ItemType Directory -Force -Path $ninjaTarget | Out-Null
    Expand-Archive -LiteralPath $archive -DestinationPath $ninjaTarget -Force
}

$llvm = $manifest.portable.llvm
$llvmTarget = Join-Path $installedRoot $llvm.installDirectory
if (-not (Test-Path -LiteralPath (Join-Path $llvmTarget 'bin\llvm-config.exe'))) {
    $archive = Get-VerifiedArchive $llvm
    $extractRoot = Join-Path $installedRoot '.extracting-llvm-22.1.6'
    if (Test-Path -LiteralPath $extractRoot) { Remove-Item -LiteralPath $extractRoot -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $extractRoot | Out-Null
    & tar.exe -xJf $archive -C $extractRoot
    if ($LASTEXITCODE -ne 0) { throw 'LLVM extraction failed' }
    $extracted = Get-ChildItem -LiteralPath $extractRoot -Directory | Select-Object -First 1
    if (-not $extracted) { throw 'LLVM archive did not contain an installation directory' }
    if (Test-Path -LiteralPath $llvmTarget) { Remove-Item -LiteralPath $llvmTarget -Recurse -Force }
    Move-Item -LiteralPath $extracted.FullName -Destination $llvmTarget
    Remove-Item -LiteralPath $extractRoot -Recurse -Force
}

$raylib = $manifest.portable.raylib
$raylibTarget = Join-Path $installedRoot $raylib.installDirectory
if (-not (Test-Path -LiteralPath (Join-Path $raylibTarget 'src\raylib.h'))) {
    $archive = Get-VerifiedArchive $raylib
    $extractRoot = Join-Path $installedRoot '.extracting-raylib-6.0'
    if (Test-Path -LiteralPath $extractRoot) { Remove-Item -LiteralPath $extractRoot -Recurse -Force }
    New-Item -ItemType Directory -Force -Path $extractRoot | Out-Null
    & tar.exe -xzf $archive -C $extractRoot
    if ($LASTEXITCODE -ne 0) { throw 'raylib extraction failed' }
    $extracted = Get-ChildItem -LiteralPath $extractRoot -Directory | Select-Object -First 1
    if (-not $extracted) { throw 'raylib archive did not contain a source directory' }
    if (Test-Path -LiteralPath $raylibTarget) { Remove-Item -LiteralPath $raylibTarget -Recurse -Force }
    Move-Item -LiteralPath $extracted.FullName -Destination $raylibTarget
    Remove-Item -LiteralPath $extractRoot -Recurse -Force
}

Write-Host 'Pinned LLVM, Ninja, and raylib dependencies installed successfully.'
