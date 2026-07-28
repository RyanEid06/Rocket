param(
    [string]$OutputDirectory = ""
)

$ErrorActionPreference = 'Stop'
$repositoryRoot = Split-Path -Parent $PSScriptRoot
$extensionSource = Join-Path $repositoryRoot 'editors\visualstudio'
$sharedEditorSource = Join-Path $repositoryRoot 'editors\vscode'

if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repositoryRoot 'out\visualstudio'
}

$resolvedOutput = [System.IO.Path]::GetFullPath($OutputDirectory)
$expectedOutputRoot = [System.IO.Path]::GetFullPath((Join-Path $repositoryRoot 'out'))
if (-not $resolvedOutput.StartsWith($expectedOutputRoot + [System.IO.Path]::DirectorySeparatorChar,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Visual Studio extension output must remain under '$expectedOutputRoot'."
}

$stagingDirectory = Join-Path $resolvedOutput 'staging'
$archivePath = Join-Path $resolvedOutput 'Rocket.Language.VisualStudio.zip'
$vsixPath = Join-Path $resolvedOutput 'Rocket.Language.VisualStudio.vsix'

if (Test-Path -LiteralPath $stagingDirectory) {
    Remove-Item -LiteralPath $stagingDirectory -Recurse -Force
}
New-Item -ItemType Directory -Path (Join-Path $stagingDirectory 'Grammars') -Force | Out-Null

Copy-Item -LiteralPath (Join-Path $extensionSource '[Content_Types].xml') -Destination $stagingDirectory
Copy-Item -LiteralPath (Join-Path $extensionSource 'source.extension.vsixmanifest') `
    -Destination (Join-Path $stagingDirectory 'extension.vsixmanifest')
Copy-Item -LiteralPath (Join-Path $extensionSource 'Rocket.pkgdef') -Destination $stagingDirectory
Copy-Item -LiteralPath (Join-Path $sharedEditorSource 'language-configuration.json') `
    -Destination (Join-Path $stagingDirectory 'rocket-language-configuration.json')
Copy-Item -LiteralPath (Join-Path $sharedEditorSource 'syntaxes\rocket.tmLanguage.json') `
    -Destination (Join-Path $stagingDirectory 'Grammars\rocket.tmLanguage.json')

if (Test-Path -LiteralPath $archivePath) {
    Remove-Item -LiteralPath $archivePath -Force
}
if (Test-Path -LiteralPath $vsixPath) {
    Remove-Item -LiteralPath $vsixPath -Force
}

Compress-Archive -Path (Join-Path $stagingDirectory '*') -DestinationPath $archivePath
Move-Item -LiteralPath $archivePath -Destination $vsixPath

Write-Output "Visual Studio extension: $vsixPath"
