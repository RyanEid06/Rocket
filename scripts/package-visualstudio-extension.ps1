param(
    [string]$OutputDirectory = ""
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$extensionProject = Join-Path $repositoryRoot 'editors\visualstudio\src\Rocket.VisualStudio\Rocket.VisualStudio.csproj'

if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repositoryRoot 'out\visualstudio'
}

$resolvedOutput = [System.IO.Path]::GetFullPath($OutputDirectory)
$expectedOutputRoot = [System.IO.Path]::GetFullPath((Join-Path $repositoryRoot 'out'))
if (-not $resolvedOutput.StartsWith($expectedOutputRoot + [System.IO.Path]::DirectorySeparatorChar,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Visual Studio extension output must remain under '$expectedOutputRoot'."
}

$vsixPath = Join-Path $resolvedOutput 'Rocket.Language.VisualStudio.vsix'
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswhere)) {
    throw 'Visual Studio Installer vswhere.exe was not found.'
}

$visualStudioRoot = & $vswhere -latest -products Microsoft.VisualStudio.Product.Community `
    -version '[18.0,19.0)' -property installationPath
if (-not $visualStudioRoot) {
    throw 'Visual Studio Community 2026 was not found.'
}

$msbuild = Join-Path $visualStudioRoot 'MSBuild\Current\Bin\MSBuild.exe'
if (-not (Test-Path -LiteralPath $msbuild)) {
    throw "MSBuild was not found at '$msbuild'."
}

$arguments = @(
    $extensionProject,
    '/restore',
    '/t:Rebuild',
    '/p:Configuration=Release',
    '/p:Platform=AnyCPU',
    "/p:VisualStudioInstallDir=$visualStudioRoot",
    '/nologo',
    '/verbosity:minimal'
)
& $msbuild @arguments
if ($LASTEXITCODE -ne 0) {
    throw "Rocket Visual Studio extension build failed with exit code $LASTEXITCODE."
}

$builtVsix = Join-Path $repositoryRoot 'out\visualstudio\build\Release\Rocket.VisualStudio.vsix'
if (-not (Test-Path -LiteralPath $builtVsix)) {
    throw "The extension build did not produce '$builtVsix'."
}
New-Item -ItemType Directory -Path $resolvedOutput -Force | Out-Null
if (-not $builtVsix.Equals($vsixPath, [System.StringComparison]::OrdinalIgnoreCase)) {
    Copy-Item -LiteralPath $builtVsix -Destination $vsixPath -Force
}

Write-Output "Visual Studio extension: $vsixPath"
