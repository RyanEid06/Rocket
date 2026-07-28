$ErrorActionPreference = 'Stop'
$dependencyRoot = $PSScriptRoot
$manifest = Get-Content -LiteralPath (Join-Path $dependencyRoot 'manifest.json') -Raw | ConvertFrom-Json
$installedRoot = Join-Path $dependencyRoot 'installed'
$llvmRoot = Join-Path $installedRoot $manifest.portable.llvm.installDirectory
$ninjaRoot = Join-Path $installedRoot $manifest.portable.ninja.installDirectory

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio Installer was not found.' }
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vsPath) { throw 'Microsoft C++ Build Tools are not installed.' }
$devShell = Join-Path $vsPath 'Common7\Tools\Launch-VsDevShell.ps1'
& $devShell -Arch amd64 -HostArch amd64 -SkipAutomaticLocation | Out-Null

$env:PATH = (Join-Path $llvmRoot 'bin') + ';' + $ninjaRoot + ';' + $env:PATH
$env:LLVM_DIR = Join-Path $llvmRoot 'lib\cmake\llvm'
$env:ROCKET_DEPS = $dependencyRoot

Write-Host "Developer environment active: LLVM $($manifest.portable.llvm.version), Ninja $($manifest.portable.ninja.version)"
