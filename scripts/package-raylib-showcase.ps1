[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',
    [string]$OutputRoot = ''
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
. (Join-Path $projectRoot 'dependencies\activate.ps1')
$preset = 'windows-' + $Configuration.ToLowerInvariant()
$packageRoot = Join-Path $projectRoot 'examples\raylib_showcase'

cmake --preset $preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build --preset $preset
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build --preset $preset --target rocket_raylib_build
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
ctest --preset $preset -L phase14 --output-on-failure
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$defaultOutputRoot = [System.IO.Path]::GetFullPath((Join-Path $projectRoot 'out\package'))
$rocket3OutputRoot = [System.IO.Path]::GetFullPath(
    (Join-Path $projectRoot 'out\rocket3-provisional'))
if ([string]::IsNullOrWhiteSpace($OutputRoot)) {
    $outputRoot = $defaultOutputRoot
} else {
    $outputRoot = [System.IO.Path]::GetFullPath((Join-Path $projectRoot $OutputRoot))
    $rocket3Prefix = $rocket3OutputRoot.TrimEnd(
        [System.IO.Path]::DirectorySeparatorChar) +
        [System.IO.Path]::DirectorySeparatorChar
    if (-not $outputRoot.StartsWith($rocket3Prefix,
            [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Custom package output must be under $rocket3OutputRoot"
    }
}
$bundle = [System.IO.Path]::GetFullPath((Join-Path $outputRoot 'rocket-raylib-showcase-1.4.0-windows-x64'))
if (-not $bundle.StartsWith($outputRoot + [System.IO.Path]::DirectorySeparatorChar,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to package outside $outputRoot"
}
if (Test-Path -LiteralPath $bundle) {
    Remove-Item -LiteralPath $bundle -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $bundle | Out-Null

$executable = Join-Path $projectRoot (
    "out\build\$preset\rocket-artifacts\raylib_showcase\.rocketc\targets\" +
    'windows-x64\rocket-raylib-showcase.exe')
if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
    throw "Showcase executable was not produced: $executable"
}
Copy-Item -LiteralPath $executable -Destination $bundle
Copy-Item -LiteralPath (Join-Path $packageRoot 'assets') -Destination $bundle -Recurse
Copy-Item -LiteralPath (Join-Path $packageRoot 'README.md') -Destination $bundle
Copy-Item -LiteralPath (Join-Path $packageRoot 'THIRD_PARTY_NOTICES.md') -Destination $bundle
Copy-Item -LiteralPath (Join-Path $projectRoot 'dependencies\installed\raylib-6.0\LICENSE') `
    -Destination (Join-Path $bundle 'RAYLIB_LICENSE.txt')

$bundlePrefix = $bundle.TrimEnd([System.IO.Path]::DirectorySeparatorChar) +
    [System.IO.Path]::DirectorySeparatorChar
$checksums = Get-ChildItem -LiteralPath $bundle -File -Recurse |
    Sort-Object FullName |
    ForEach-Object {
        if (-not $_.FullName.StartsWith($bundlePrefix,
                [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to checksum a file outside the package bundle: $($_.FullName)"
        }
        $relative = $_.FullName.Substring($bundlePrefix.Length).Replace('\', '/')
        $hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        "$hash  $relative"
    }
$checksums | Set-Content -LiteralPath (Join-Path $bundle 'SHA256SUMS.txt') -Encoding ascii

$archive = $bundle + '.zip'
if (Test-Path -LiteralPath $archive) { Remove-Item -LiteralPath $archive -Force }
Compress-Archive -LiteralPath $bundle -DestinationPath $archive -CompressionLevel Optimal
Write-Output "Packaged Rocket raylib showcase: $archive"
