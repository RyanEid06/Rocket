$ErrorActionPreference = 'Stop'
$dependencyRoot = $PSScriptRoot
$manifest = Get-Content -LiteralPath (Join-Path $dependencyRoot 'manifest.json') -Raw | ConvertFrom-Json
$installedRoot = Join-Path $dependencyRoot 'installed'
$platform = $manifest.platforms.'windows-x64'
$llvmRoot = Join-Path $installedRoot $platform.llvm.installDirectory
$ninjaRoot = Join-Path $installedRoot $platform.ninja.installDirectory

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio Installer was not found.' }
$vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
# Visual Studio can temporarily report an installed instance as incomplete
# while its installer services refresh. The native tools remain usable in that
# state, and `-all` is the documented way to include such instances.
if (-not $vsPath) {
    $vsPath = & $vswhere -all -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath |
        Select-Object -First 1
}
if (-not $vsPath) { throw 'Microsoft C++ Build Tools are not installed.' }
$devShell = Join-Path $vsPath 'Common7\Tools\Launch-VsDevShell.ps1'
try {
    & $devShell -Arch amd64 -HostArch amd64 -SkipAutomaticLocation *> $null
}
catch {
    # VsDevCmd below is the supported fallback for installations whose
    # PowerShell developer-shell launcher cannot initialize in this host.
}

# Some Visual Studio releases can leave Launch-VsDevShell unable to import the
# environment even though the installation and native tools are healthy. Fall
# back to VsDevCmd and import its environment into this PowerShell process.
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    $devCmd = Join-Path $vsPath 'Common7\Tools\VsDevCmd.bat'
    if (-not (Test-Path -LiteralPath $devCmd)) { throw 'Visual Studio developer command script was not found.' }
    $environmentLines = & $env:ComSpec /s /c "`"$devCmd`" -arch=x64 -host_arch=x64 >nul && set"
    if ($LASTEXITCODE -ne 0) { throw 'Visual Studio developer environment could not be activated.' }
    $importedNames = @{}
    foreach ($line in $environmentLines) {
        $separator = $line.IndexOf('=')
        if ($separator -gt 0) {
            $name = $line.Substring(0, $separator)
            $value = $line.Substring($separator + 1)
            # Windows environment names are case-insensitive, but `set` can
            # expose inherited PATH/Path duplicates. Keep its first (updated)
            # value instead of overwriting it with a stale duplicate.
            if (-not $importedNames.ContainsKey($name)) {
                Set-Item -LiteralPath "Env:$name" -Value $value
                $importedNames[$name] = $true
            }
        }
    }
}

if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    throw 'MSVC cl.exe was not found after developer environment activation.'
}

$msvcCompiler = Get-Command cl.exe -ErrorAction Stop
$msvcLibrarian = Get-Command lib.exe -ErrorAction Stop
$msvcLinker = Get-Command link.exe -ErrorAction Stop
$env:ROCKET_MSVC_COMPILER = $msvcCompiler.Source
$env:ROCKET_MSVC_LIBRARIAN = $msvcLibrarian.Source
$env:ROCKET_MSVC_LINKER = $msvcLinker.Source

$env:PATH = (Join-Path $llvmRoot 'bin') + ';' + $ninjaRoot + ';' + $env:PATH
$env:LLVM_DIR = Join-Path $llvmRoot 'lib\cmake\llvm'
$env:ROCKET_DEPS = $dependencyRoot

Write-Host "Developer environment active: LLVM $($platform.llvm.version), Ninja $($platform.ninja.version)"
