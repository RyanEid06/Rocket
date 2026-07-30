[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$Destination,
    [string]$Name = ''
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
$templateRoot = Join-Path $projectRoot 'examples\raylib_showcase'
$target = [System.IO.Path]::GetFullPath($Destination)
if (Test-Path -LiteralPath $target) {
    if (Get-ChildItem -LiteralPath $target -Force | Select-Object -First 1) {
        throw "Destination is not empty: $target"
    }
} else {
    New-Item -ItemType Directory -Path $target | Out-Null
}

foreach ($directory in @('assets', 'native', 'src', 'tests')) {
    Copy-Item -LiteralPath (Join-Path $templateRoot $directory) -Destination $target -Recurse
}
Copy-Item -LiteralPath (Join-Path $templateRoot 'rocket.toml') -Destination $target
Copy-Item -LiteralPath (Join-Path $templateRoot 'CMakeLists.txt') -Destination $target
Copy-Item -LiteralPath (Join-Path $templateRoot 'README.md') -Destination $target
Copy-Item -LiteralPath (Join-Path $templateRoot 'THIRD_PARTY_NOTICES.md') -Destination $target

if (-not $Name) { $Name = Split-Path $target -Leaf }
if ($Name -notmatch '^[A-Za-z_][A-Za-z0-9_-]*$') {
    throw "Invalid Rocket package name: $Name"
}
$manifestPath = Join-Path $target 'rocket.toml'
$manifest = Get-Content -LiteralPath $manifestPath -Raw
$manifest = $manifest.Replace('name = "raylib_showcase"', "name = `"$Name`"")
$manifest = $manifest.Replace('name = "rocket-raylib-showcase"', "name = `"$Name`"")
Set-Content -LiteralPath $manifestPath -Value $manifest -Encoding ascii

Write-Output "Created Rocket raylib application template at $target"
Write-Output 'Configure Rocket from the repository CMake project to generate bindings and native libraries.'
