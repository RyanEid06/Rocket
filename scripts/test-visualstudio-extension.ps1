[CmdletBinding()]
param(
    [switch]$SkipPackage
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$visualStudioRoot = & $vswhere -latest -products Microsoft.VisualStudio.Product.Community `
    -version '[18.0,19.0)' -property installationPath
if (-not $visualStudioRoot) {
    throw 'Visual Studio Community 2026 was not found.'
}
$msbuild = Join-Path $visualStudioRoot 'MSBuild\Current\Bin\MSBuild.exe'
$testProject = Join-Path $repositoryRoot 'editors\visualstudio\tests\Rocket.VisualStudio.CoreTests\Rocket.VisualStudio.CoreTests.csproj'

& $msbuild $testProject /restore /t:Rebuild /p:Configuration=Release /p:Platform=AnyCPU /nologo /verbosity:minimal
if ($LASTEXITCODE -ne 0) {
    throw "Rocket Visual Studio focused test build failed with exit code $LASTEXITCODE."
}
$testExecutable = Join-Path $repositoryRoot 'out\visualstudio\tests\Release\Rocket.VisualStudio.CoreTests.exe'
& $testExecutable
if ($LASTEXITCODE -ne 0) {
    throw "Rocket Visual Studio focused tests failed with exit code $LASTEXITCODE."
}

if (-not $SkipPackage) {
    & (Join-Path $PSScriptRoot 'package-visualstudio-extension.ps1')
}
$vsix = Join-Path $repositoryRoot 'out\visualstudio\Rocket.Language.VisualStudio.vsix'
if (-not (Test-Path -LiteralPath $vsix)) {
    throw "Rocket VSIX was not found at '$vsix'."
}

Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [System.IO.Compression.ZipFile]::OpenRead($vsix)
try {
    $required = @(
        'extension.vsixmanifest',
        'Rocket.VisualStudio.dll',
        'Rocket.VisualStudio.pkgdef',
        'Rocket.pkgdef',
        'language-configuration.json',
        'Grammars/rocket.tmLanguage.json'
    )
    $entryNames = @($archive.Entries | ForEach-Object FullName)
    foreach ($name in $required) {
        if ($name -notin $entryNames) {
            throw "Rocket VSIX is missing '$name'."
        }
    }
    $manifestEntry = $archive.GetEntry('extension.vsixmanifest')
    $reader = New-Object System.IO.StreamReader($manifestEntry.Open())
    try {
        [xml]$manifest = $reader.ReadToEnd()
    } finally {
        $reader.Dispose()
    }
    $namespace = New-Object System.Xml.XmlNamespaceManager($manifest.NameTable)
    $namespace.AddNamespace('v', 'http://schemas.microsoft.com/developer/vsx-schema/2011')
    $identity = $manifest.SelectSingleNode('/v:PackageManifest/v:Metadata/v:Identity', $namespace)
    if ($identity.Id -ne 'Rocket.Language.VisualStudio' -or $identity.Version -ne '2.0.3') {
        throw 'Rocket VSIX did not preserve the extension identity at version 2.0.3.'
    }
    $assets = @($manifest.SelectNodes('/v:PackageManifest/v:Assets/v:Asset', $namespace))
    if (-not ($assets | Where-Object Type -eq 'Microsoft.VisualStudio.MefComponent')) {
        throw 'Rocket VSIX is missing the LSP MEF component asset.'
    }
    foreach ($entry in $archive.Entries) {
        if ($entry.Length -gt 2MB) {
            continue
        }
        $stream = $entry.Open()
        $memory = $null
        try {
            $memory = New-Object System.IO.MemoryStream
            $stream.CopyTo($memory)
            $text = [System.Text.Encoding]::UTF8.GetString($memory.ToArray())
            if ($text.Contains($repositoryRoot)) {
                throw "Rocket VSIX entry '$($entry.FullName)' contains a machine-specific repository path."
            }
        } finally {
            if ($memory) { $memory.Dispose() }
            $stream.Dispose()
        }
    }
} finally {
    $archive.Dispose()
}

Write-Output 'Rocket Visual Studio extension validation passed.'
