[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
. (Join-Path $projectRoot 'dependencies\activate.ps1')

# Visual Studio's bundled CMake writes these cache values into generated CMake
# source. Forward slashes keep Windows paths valid when they are quoted there.
$env:ROCKET_MSVC_COMPILER = $env:ROCKET_MSVC_COMPILER.Replace('\', '/')
$env:ROCKET_MSVC_LIBRARIAN = $env:ROCKET_MSVC_LIBRARIAN.Replace('\', '/')
$env:ROCKET_MSVC_LINKER = $env:ROCKET_MSVC_LINKER.Replace('\', '/')
$env:LLVM_DIR = $env:LLVM_DIR.Replace('\', '/')

# Visual Studio 2026 stores solutionless-workspace launch configuration under
# the ignored .vs directory. Keep the source-controlled template elsewhere and
# refresh the local copy on every supported launch.
$visualStudioState = Join-Path $projectRoot '.vs'
$launchTemplate = Join-Path $projectRoot 'editors\visualstudio\launch.vs.json'
$launchConfiguration = Join-Path $visualStudioState 'launch.vs.json'
New-Item -ItemType Directory -Path $visualStudioState -Force | Out-Null
Copy-Item -LiteralPath $launchTemplate -Destination $launchConfiguration -Force

$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$visualStudioRoot = & $vswhere -latest -products * -property installationPath
if (-not $visualStudioRoot) {
    throw 'Visual Studio was not found.'
}

$devenv = Join-Path $visualStudioRoot 'Common7\IDE\devenv.exe'
if (-not (Test-Path -LiteralPath $devenv)) {
    throw "Visual Studio IDE was not found at $devenv"
}

$quotedProjectRoot = '"' + $projectRoot + '"'
Start-Process -FilePath $devenv -ArgumentList @($quotedProjectRoot) -WorkingDirectory $projectRoot
